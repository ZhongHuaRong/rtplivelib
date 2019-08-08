#include "cameracapture.h"
#include "../core/time.h"
#include "../core/logger.h"
#include "../core/except.h"
extern "C" {
    #include <libavformat/avformat.h>
    #include <libavdevice/avdevice.h>
	#include <libavutil/dict.h>
}
#if defined (WIN64)
#include <dshow.h>
#include "../core/stringformat.h"
#endif

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
	//初始化的时候将设备设置成默认设备
	set_default_device();
	
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

void CameraCapture::set_video_size(const VideoSize &size) noexcept
{
    if(_size == size)
        return;
    _size = size;
    open_device();
}

std::map<std::string,std::string> CameraCapture::get_all_device_info() noexcept(false)
{
#if defined (unix)
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
#elif defined (WIN64)
    ICreateDevEnum *pDevEnum{nullptr};
    IEnumMoniker *pEnum {nullptr};
    HRESULT hr = 0;
    hr = CoCreateInstance(CLSID_SystemDeviceEnum,
                          nullptr,
                          CLSCTX_INPROC_SERVER,
                          IID_ICreateDevEnum,
                          reinterpret_cast<void**>(&pDevEnum));
    if (SUCCEEDED(hr))
    {
        hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory,&pEnum,0);
        if (hr == S_OK)
        {
            //使用IEnumMoniker接口枚举所有的设备标识
            IMoniker *pMoniker{nullptr};
            ULONG cFetched{0};
            std::map<std::string,std::string> info_map;
            
            while (pEnum->Next(1, &pMoniker, &cFetched) == S_OK)
            {
                IPropertyBag* pPropBag{nullptr};
                hr = pMoniker->BindToStorage(nullptr, 
                                             nullptr, 
                                             IID_IPropertyBag,
                                             reinterpret_cast<void**>(&pPropBag));
                if(SUCCEEDED(hr)){
                    BSTR devicePath{nullptr};
                    std::pair<std::string,std::string> pair;
                    
                    hr = pMoniker->GetDisplayName(nullptr, nullptr, &devicePath);
                    if(SUCCEEDED(hr)){
                        //获取设备唯一ID
                        //由于ffmpeg是根据设备名字更换设备的，所以这里不是最主要的
                        pair.first = core::StringFormat::WString2String(devicePath);
                    }
                    
                    VARIANT varName;
                    varName.vt = VT_BSTR;
                    VariantInit(&varName);
                    hr = pPropBag->Read(L"FriendlyName", &varName, nullptr);
                    if (SUCCEEDED(hr)){
                        //获取设备友好名字
                        pair.second = core::StringFormat::WString2String(varName.bstrVal);
                        info_map.insert(pair);
                    }
                    VariantClear(&varName);
                    pPropBag->Release();
                }
                pMoniker->Release();
            }
            pDevEnum->Release();
            return info_map;
        }
        pDevEnum->Release();
    } 
    throw std::runtime_error("Unknown");
#endif
}

bool CameraCapture::set_default_device() noexcept
{
	//说是设置成默认设备，其实是选择第一个设备
	auto list = get_all_device_info();
	if(list.size() == 0){
		current_device_info.first.clear();
		current_device_info.second.clear();
		return false;
	}
	auto first = list.begin();
	current_device_info.first = first->first;
	current_device_info.second = first->second;
	return true;
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
	AVDictionary *options = nullptr;
	av_dict_set(&options,"framerate",std::to_string(_fps).c_str(),0);
    av_dict_set(&options,"video_size",_size.to_string().c_str(),0);
	
	std::lock_guard<std::mutex> lk(d_ptr->fmt_ctx_mutex);
	if(d_ptr->fmtContxt != nullptr){
		avformat_close_input(&d_ptr->fmtContxt);
	}
	constexpr char api[] = "device_manager::CameraCapture::open_device";
#if defined (WIN32)
	
	auto n = avformat_open_input(&d_ptr->fmtContxt,("video=" + current_device_info.second).c_str(),d_ptr->ifmt,&options);
//    auto n = avformat_open_input(&d_ptr->fmtContxt,"video=USB2.0 VGA UVC WebCam",d_ptr->ifmt,&options);
#elif defined (unix)
	bool n;
	try {
		auto ptr = current_device_info.second.c_str();
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
