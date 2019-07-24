#include "cameracapture.h"
#include "../core/time.h"
#include "../core/logger.h"
#include "../core/except.h"
extern "C" {
    #include <libavformat/avformat.h>
    #include <libavdevice/avdevice.h>
	#include <libavutil/dict.h>
}

namespace rtplivelib {

namespace device_manager {

class CameraCapturePrivateData{
public:
	AVInputFormat *ifmt{nullptr};
	AVFormatContext *fmtContxt{nullptr};
	std::mutex fmt_ctx_mutex;
	AVPacket *packet{nullptr};
};

#if defined (WIN32)
	static constexpr char format_name[] = "dshow";
#elif defined (unix)
	static constexpr char format_name[] = "v4l2";
#endif

CameraCapture::CameraCapture() :
	AbstractCapture(AbstractCapture::CaptureType::Camera),
	_fps(15),
	d_ptr(new CameraCapturePrivateData)
{
	avdevice_register_all();
	
	d_ptr->ifmt = av_find_input_format(format_name);
	
	//这里应该不会出现这种情况,但是ffmpeg库的编译出问题的话，这里就会触发了
	if(d_ptr->ifmt == nullptr){
		core::Logger::Print_APP_Info(core::MessageNum::InputFormat_format_not_found,
									 "device_manager::CameraCapture::CameraCapture",
									 LogLevel::ERROR_LEVEL,
									 format_name);
	}
	
	//这里只是为了设置当前名字
	AVDeviceInfoList *info_list = nullptr;
	avdevice_list_input_sources(d_ptr->ifmt,nullptr,nullptr,&info_list);
	if(info_list != nullptr && info_list->nb_devices != 0){
		if(info_list->default_device == -1){
			current_device_info.first = info_list->devices[0]->device_description;
			current_device_info.second = info_list->devices[0]->device_description;
		}
		else{
			current_device_info.first = info_list->devices[info_list->default_device]->device_description;
			current_device_info.second = info_list->devices[info_list->default_device]->device_description;
		}
	}
	
	/*在子类初始化里开启线程*/
	start_thread();
}

CameraCapture::~CameraCapture()
{
	stop_capture();
	exit_thread();
	av_packet_free(&d_ptr->packet);
	delete d_ptr;
}

void CameraCapture::set_fps(int value) {
	if (value == _fps)
		return;
	_fps = value;
	open_device();
}

std::map<std::string,std::string> CameraCapture::get_all_device_info() noexcept(false)
{
	if(d_ptr->ifmt == nullptr){
		throw core::uninitialized_error("AVInputFormat(ifmt)");
	}
	
	AVDeviceInfoList *info_list = nullptr;
	avdevice_list_input_sources(d_ptr->ifmt,nullptr,nullptr,&info_list);
	if(info_list){
		std::map<std::string,std::string> info_map;
		for(auto index = 0;index < info_list->nb_devices;++index){
			std::pair<std::string,std::string> pair;
			pair.first = info_list->devices[index]->device_description;
			pair.second = info_list->devices[index]->device_name;
			info_map.insert(pair);
		}
		return info_map;
	}
	else {
		//这里不判断返回值了，因为我知道失败的情况一般是函数未实现
		throw core::func_not_implemented_error(core::MessageString[int(core::MessageNum::Device_info_failed)]);
	}
}

bool CameraCapture::set_default_device() noexcept
{
	//先不实现
}

bool CameraCapture::set_current_device(std::string device_id) noexcept
{
	//只区分设备id，不区分设备名字
	if(device_id.compare(current_device_info.second) == 0){
		return true;
	}
	auto temp = current_device_info;
	current_device_info.first = device_id;
	current_device_info.second = device_id;
	auto result = open_device();
	if(!result){
		current_device_info = temp;
		core::Logger::Print_APP_Info(core::MessageNum::Device_change_success,
									 "device_manager::CameraCapture::set_current_device",
									 LogLevel::ERROR_LEVEL,
									 current_device_info.first.c_str());
	}
	return result;
}

CameraCapture::SharedPacket CameraCapture::on_start() noexcept {
	if(d_ptr->fmtContxt == nullptr){
		if(!open_device()){
			stop_capture();
			return nullptr;
		}
	}
	constexpr char api[] = "device_manager::CameraCapture::on_start";
	
	//开始捕捉前，睡眠1ms
	//防止刚解锁就拿到锁，其他线程饥饿
	this->sleep(1);
	
	if(d_ptr->packet == nullptr){
		d_ptr->packet = av_packet_alloc();
		if(d_ptr->packet == nullptr){
			core::Logger::Print_APP_Info(core::MessageNum::FramePacket_packet_alloc_failed,
										 api,
										 LogLevel::WARNING_LEVEL);
			return nullptr;
		}
	}
	av_packet_unref(d_ptr->packet);
	std::lock_guard<std::mutex> lk(d_ptr->fmt_ctx_mutex);
	
	auto ret = av_read_frame(d_ptr->fmtContxt, d_ptr->packet);
	if( ret < 0){
		core::Logger::Print_APP_Info(core::MessageNum::Device_read_frame_failed,
									 api,
									 LogLevel::WARNING_LEVEL);
		core::Logger::Print_RTP_Info(ret,api,LogLevel::WARNING_LEVEL);
		return nullptr;
	}
	
	auto codec = d_ptr->fmtContxt->streams[d_ptr->packet->stream_index]->codecpar;
	
	auto ptr = FramePacket::Make_Shared();
	if(ptr == nullptr){
		core::Logger::Print_APP_Info(core::MessageNum::FramePacket_alloc_failed,
									 api,
									 LogLevel::WARNING_LEVEL);
		return ptr;
	}
	ptr->format.width = codec->width;
	ptr->format.height = codec->height;
	ptr->format.bits = 16;
	ptr->linesize[0] = ptr->format.width * 2;
	ptr->format.pixel_format = AV_PIX_FMT_YUYV422;
	ptr->pts = d_ptr->packet->pts;
	ptr->dts = d_ptr->packet->dts;
	ptr->format.frame_rate = _fps;
	
	//packet在close input format后就失效
	//所以需要拷贝一份
	ptr->data[0] = static_cast<uint8_t*>(av_malloc(static_cast<size_t>(d_ptr->packet->size)));
	if(ptr->data[0] == nullptr){
		core::Logger::Print_APP_Info(core::MessageNum::FramePacket_data_alloc_failed,
									 api,
									 LogLevel::WARNING_LEVEL);
		ptr->size = 0;
		return ptr;
	}
	memcpy(ptr->data[0],d_ptr->packet->data,static_cast<size_t>(d_ptr->packet->size));
	ptr->size = d_ptr->packet->size;
	return ptr;
}

void CameraCapture::on_stop()  noexcept{
	std::lock_guard<std::mutex> lk(d_ptr->fmt_ctx_mutex);
	if(d_ptr->fmtContxt == nullptr)
		return;
	avformat_close_input(&d_ptr->fmtContxt);
	core::Logger::Print_APP_Info(core::MessageNum::InputFormat_context_close,
								 "device_manager::CameraCapture::on_stop",
								 LogLevel::INFO_LEVEL);
}

bool CameraCapture::open_device() noexcept
{
	char fps_char[5];
	sprintf(fps_char,"%d",_fps);
	AVDictionary *options = nullptr;
	av_dict_set(&options,"framerate",fps_char,0);
	av_dict_set(&options,"video_size","640x480",0); 
	
	std::lock_guard<std::mutex> lk(d_ptr->fmt_ctx_mutex);
	if(d_ptr->fmtContxt != nullptr){
		avformat_close_input(&d_ptr->fmtContxt);
	}
	constexpr char api[] = "device_manager::CameraCapture::open_device";
#if defined (WIN32)
	auto n = avformat_open_input(&d_ptr->fmtContxt,"video=HD Pro Webcam C920",d_ptr->ifmt,&options);
//    auto n = avformat_open_input(&d_ptr->fmtContxt,"video=USB2.0 VGA UVC WebCam",d_ptr->ifmt,&options);
#elif defined (unix)
	bool n;
	try {
		auto ptr = get_all_device_info()[current_device_info.second].c_str();
		n = avformat_open_input(&d_ptr->fmtContxt, ptr,
								d_ptr->ifmt,&options);
	} catch (const core::func_not_implemented_error& exception) {
		return false;
	}
#endif
	if( n != 0 ){
		core::Logger::Print_APP_Info(core::MessageNum::InputFormat_context_open,
									 api,
									 LogLevel::WARNING_LEVEL,
									 "false");
		core::Logger::Print_FFMPEG_Info(n,
									 api,
									 LogLevel::WARNING_LEVEL);
	}else {
		core::Logger::Print_APP_Info(core::MessageNum::InputFormat_context_open,
									 api,
									 LogLevel::INFO_LEVEL,
									 "true");
	}
	return n == 0;
}

}//namespace device_manager

}//namespace rtplivelib
