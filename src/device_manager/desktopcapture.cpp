#include "desktopcapture.h"
#include "../core/logger.h"
extern "C" {
    #include <libavformat/avformat.h>
    #include <libavdevice/avdevice.h>
	#include <libavutil/dict.h>
}

namespace rtplivelib {

namespace device_manager {

class DesktopCapturePrivateData {
public:
	AVInputFormat *ifmt{nullptr};
	AVFormatContext *fmtContxt{nullptr};
	std::mutex _fmt_ctx_mutex;
	AVPacket *packet{nullptr};
};

#if defined (WIN64)
	static constexpr char format_name[] = "gdigrab";
#elif defined (unix)
	static constexpr char format_name[] = "fbdev";
#endif

DesktopCapture::DesktopCapture() :
	AbstractCapture(AbstractCapture::CaptureType::Desktop),
	_fps(15),
	d_ptr(new DesktopCapturePrivateData)
{
	avdevice_register_all();
	
	d_ptr->ifmt = av_find_input_format(format_name);
	//这里应该不会出现这种情况,但是ffmpeg库的编译出问题的话，这里就会触发了
	if(d_ptr->ifmt == nullptr){
		core::Logger::Print_APP_Info(core::Result::InputFormat_format_not_found,
									 __PRETTY_FUNCTION__,
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
	
#if defined (WIN64)
	current_device_info.first = "desktop";
	current_device_info.second = current_device_info.first;
#endif
	
	/*在子类初始化里开启线程*/
	start_thread();
}

DesktopCapture::~DesktopCapture()
{
	stop_capture();
	exit_thread();
	av_packet_free(&d_ptr->packet);
	delete d_ptr;
}

void DesktopCapture::set_fps(int value)  noexcept{
	if (value == _fps)
		return;
	_fps = value;
	open_device();
}

void DesktopCapture::set_window(const uint64_t & id)  noexcept{
	_wid = id;
}

std::map<DesktopCapture::device_id,DesktopCapture::device_name> DesktopCapture::get_all_device_info() noexcept(false)
{
	if(d_ptr->ifmt == nullptr){
		throw core::uninitialized_error("AVInputFormat(ifmt)");
	}
	
	AVDeviceInfoList *info_list = nullptr;
	avdevice_list_input_sources(d_ptr->ifmt,nullptr,nullptr,&info_list);
	if(info_list){
		std::map<device_id,device_name> info_map;
		for(auto index = 0;index < info_list->nb_devices;++index){
			std::pair<device_id,device_name> pair;
			pair.first = info_list->devices[index]->device_description;
			pair.second = info_list->devices[index]->device_name;
			info_map.insert(pair);
		}
		return info_map;
	}
	else {
		//这里不判断返回值了，因为我知道失败的情况一般是函数未实现
		throw core::func_not_implemented_error(core::MessageString[int(core::Result::Device_info_failed)]);
	}
}

bool DesktopCapture::set_current_device(device_id device_id) noexcept
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
		core::Logger::Print_APP_Info(core::Result::Device_change_success,
									 __PRETTY_FUNCTION__,
									 LogLevel::ERROR_LEVEL,
									 current_device_info.first.c_str());
	}
	return result;
}

bool DesktopCapture::set_default_device() noexcept
{
	//这里没有实现
    //预期应该是设置成主屏
    return false;
}

AbstractCapture::SharedPacket DesktopCapture::on_start() noexcept
{
	if(d_ptr->fmtContxt == nullptr){
		if(!open_device()){
			stop_capture();
			return nullptr;
		}
	}
	
	//开始捕捉前，睡眠1ms
	//防止刚解锁就拿到锁，其他线程饥饿
	this->sleep(1);
	
	if(d_ptr->packet == nullptr){
		d_ptr->packet = av_packet_alloc();
		if(d_ptr->packet == nullptr){
			core::Logger::Print_APP_Info(core::Result::FramePacket_packet_alloc_failed,
										 __PRETTY_FUNCTION__,
										 LogLevel::WARNING_LEVEL);
			return nullptr;
		}
	}
	
	/*这里说明一下，为什么不需要用计时器来算每帧间隔*/
	/*这里的av_read_frame函数会阻塞，一直等到有帧可读才返回，所以不需要自己设定间隔*/
	/*加锁是怕主线程在重启设备fmtContxt*/
	std::lock_guard<std::mutex> lk(d_ptr->_fmt_ctx_mutex);
	av_packet_unref(d_ptr->packet);
	auto ret = av_read_frame(d_ptr->fmtContxt, d_ptr->packet);
	if( ret < 0){
		core::Logger::Print_APP_Info(core::Result::Device_read_frame_failed,
									 __PRETTY_FUNCTION__,
									 LogLevel::WARNING_LEVEL);
		core::Logger::Print_RTP_Info(ret,
									 __PRETTY_FUNCTION__,
									 LogLevel::WARNING_LEVEL);
		return nullptr;
	}
		
	auto ptr = FramePacket::Make_Shared();
	if(ptr == nullptr){
		core::Logger::Print_APP_Info(core::Result::FramePacket_alloc_failed,
									 __PRETTY_FUNCTION__,
									 LogLevel::WARNING_LEVEL);
		return ptr;
	}
#if defined (WIN64)
	//去掉bmp头结构54字节
	ptr->data->size = d_ptr->packet->size - 54;
#elif defined (unix)
	ptr->size = d_ptr->packet->size;
#endif
	
	//下面一步是为了赋值width和height，windows下面不能正确读取，需要计算
#if defined (WIN64)
	ptr->data->copy_data(d_ptr->packet->data + 54,static_cast<size_t>(ptr->data->size));
	/*width在头结构地址18偏移处，详情参考BMP头结构*/
	memcpy(&ptr->format.width,d_ptr->packet->data + 18,4);
	/*height在头结构地址22偏移处，不过总为0*/
	/*所以采用数据大小除以宽和像素位宽(RGB32是4位)得到高*/
	memcpy(&ptr->format.height,d_ptr->packet->data + 2,4);
	ptr->format.height = (ptr->format.height - 54) / ptr->format.width / 4;
#elif defined (unix)
	ptr->data->copy_data(d_ptr->packet->data,static_cast<size_t>(d_ptr->packet->size));
	auto codec = d_ptr->fmtContxt->streams[d_ptr->packet->stream_index]->codecpar;
	ptr->format.width = codec->width;
	ptr->format.height = codec->height;
	
#endif
	ptr->format.pixel_format = AV_PIX_FMT_BGRA;
	ptr->format.bits = 32;
	ptr->pts = d_ptr->packet->pts;
	ptr->dts = d_ptr->packet->dts;
	ptr->format.frame_rate = _fps;
	ptr->flag = d_ptr->packet->flags;
	ptr->data->linesize[0] = ptr->format.width * 4;
	
	return ptr;
}

void DesktopCapture::on_stop() noexcept
{
	std::lock_guard<std::mutex> lk(d_ptr->_fmt_ctx_mutex);
	if(d_ptr->fmtContxt == nullptr)
		return;
	avformat_close_input(&d_ptr->fmtContxt);
	core::Logger::Print_APP_Info(core::Result::InputFormat_context_close,
								 __PRETTY_FUNCTION__,
								 LogLevel::INFO_LEVEL);
}

bool DesktopCapture::open_device() noexcept
{
	AVDictionary *options = nullptr;
	//windows采用GDI采集桌面屏幕，比较吃资源，以后需要更换API
	//设置帧数，经过测试，貌似最高是30，而且太吃资源，有空肯定换了这个API
	char fps_char[5];
	sprintf(fps_char,"%d",_fps);
	av_dict_set(&options,"framerate",fps_char,0);
	
	std::lock_guard<std::mutex> lk(d_ptr->_fmt_ctx_mutex);
	if(d_ptr->fmtContxt != nullptr){
		avformat_close_input(&d_ptr->fmtContxt);
	}
#if defined (WIN64)
	/*暂时只考虑截取屏幕的情况，截取窗口和另外的屏幕以后再考虑*/
	auto n = avformat_open_input(&d_ptr->fmtContxt, "desktop", d_ptr->ifmt, &options);
#elif defined (unix)
	auto ptr = get_all_device_info()[current_device_info.second].c_str();
	auto n = avformat_open_input(&d_ptr->fmtContxt, ptr, 
								 d_ptr->ifmt, &options);
#endif
	if( n != 0 ){
		core::Logger::Print_APP_Info(core::Result::InputFormat_context_open,
									 __PRETTY_FUNCTION__,
									 LogLevel::WARNING_LEVEL,
									 "false");
		core::Logger::Print_FFMPEG_Info(n,
										__PRETTY_FUNCTION__,
										LogLevel::WARNING_LEVEL);
	}else {
		core::Logger::Print_APP_Info(core::Result::InputFormat_context_open,
									 __PRETTY_FUNCTION__,
									 LogLevel::INFO_LEVEL,
									 "true");
	}
	return n == 0;
}

} // namespace device_manager

} // namespace rtplivelib
