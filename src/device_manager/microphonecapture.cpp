#include "microphonecapture.h"
#include "wasapi.h"

namespace rtplivelib {

namespace device_manager {

class MicrophoneCapturePrivateData{
public:
#if defined (WIN64)
	WASAPI was;
	static constexpr WASAPI::FlowType FT{WASAPI::CAPTURE};
#endif
	std::mutex fmt_ctx_mutex;
	std::string fmt_name;
};

//////////////////////////////////////////////////////////////////////////////////////////

MicrophoneCapture::MicrophoneCapture() :
	AbstractCapture(AbstractCapture::CaptureType::Microphone),
	d_ptr(new MicrophoneCapturePrivateData)
{
	d_ptr->was.set_default_device(MicrophoneCapturePrivateData::FT);
	
	try {
		auto && list = d_ptr->was.get_device_info(MicrophoneCapturePrivateData::FT);
		
		list.
	} catch (...) {
		
	}
	
#if defined (WIN32)
	d_ptr->ifmt = av_find_input_format("dshow");
#elif defined (unix)
	d_ptr->ifmt = av_find_input_format("alsa");
	
	//这里只是为了设置当前名字
	AVDeviceInfoList *info_list = nullptr;
	avdevice_list_input_sources(d_ptr->ifmt,nullptr,nullptr,&info_list);
	if(info_list != nullptr && info_list->nb_devices != 0){
		if(info_list->default_device == -1)
			current_device_name = info_list->devices[0]->device_description;
		else
			current_device_name = info_list->devices[info_list->default_device]->device_description;
	}
#endif
	/*在子类初始化里开启线程*/
	start_thread();
}

MicrophoneCapture::~MicrophoneCapture() 
{
	exit_thread();
	delete d_ptr;
}

std::map<std::string,std::string> MicrophoneCapture::get_all_device_info() noexcept
{
	std::map<std::string,std::string> info_map;
	
	if(d_ptr->ifmt == nullptr){
		std::cout << "not found input format" << std::endl;
		return info_map;
	}
	
	AVDeviceInfoList *info_list = nullptr;
	auto n = avdevice_list_input_sources(d_ptr->ifmt,nullptr,nullptr,&info_list);
//	PrintErrorMsg(n);
	if(info_list){
		for(auto index = 0;index < info_list->nb_devices;++index){
			std::pair<std::string,std::string> pair;
			pair.first = info_list->devices[index]->device_description;
			pair.second = info_list->devices[index]->device_name;
			info_map.insert(pair);
		}
	}
	return info_map;
}

/**
 * @brief set_current_device_name
 * 根据名字设置当前设备，成功则返回true，失败则返回false 
 */
bool MicrophoneCapture::set_current_device_name(std::string name) noexcept
{
	if(name.compare(current_device_name) == 0)
		return true;
	auto temp = current_device_name;
	current_device_name = name;
	auto result = open_device();
	if(!result){
		current_device_name = temp;
	}
	return result;
}

/**
 * @brief on_start
 * 开始捕捉音频后的回调
 */
AbstractCapture::SharedPacket MicrophoneCapture::on_start() noexcept
{
	if(d_ptr->fmtContxt == nullptr){
		if(!open_device()){
			stop_capture();
			return nullptr;
		}
	}
	
	d_ptr->fmt_ctx_mutex.lock();
	AVPacket *packet = av_packet_alloc();
	av_read_frame(d_ptr->fmtContxt, packet);
		
	auto ptr = FramePacket::Make_Shared();
	ptr->packet = packet;
	ptr->data[0] = packet->data;
	ptr->size = packet->size;
	auto codec = d_ptr->fmtContxt->streams[packet->stream_index]->codecpar;
	ptr->format.channels = codec->channels;
	ptr->format.sample_rate = codec->sample_rate;
	ptr->format.pixel_format = codec->format;
	
	d_ptr->fmt_ctx_mutex.unlock();
	return ptr;
}

/**
 * @brief on_stop
 * 结束捕捉音频后的回调
 */
void MicrophoneCapture::on_stop() noexcept 
{
	if(d_ptr->fmtContxt == nullptr)
		return;
	d_ptr->fmt_ctx_mutex.lock();
	avformat_close_input(&d_ptr->fmtContxt);
	d_ptr->fmt_ctx_mutex.unlock();
}

bool MicrophoneCapture::open_device() noexcept
{
	AVDictionary *options = nullptr;
	av_dict_set(&options,"sample_rate","48000",0);
	av_dict_set(&options,"channels","2",0); 
	
	std::lock_guard<std::mutex> lk(d_ptr->fmt_ctx_mutex);
	if(d_ptr->fmtContxt != nullptr){
		avformat_close_input(&d_ptr->fmtContxt);
	}
#if defined (WIN32)
	auto n = avformat_open_input(&d_ptr->fmtContxt,"audio=麦克风(HD Pro Webcam C920)",d_ptr->ifmt,&options);
#elif defined (unix)
	auto name = get_all_device_info()[current_device_name];
	auto n = avformat_open_input(&d_ptr->fmtContxt, name.c_str(),d_ptr->ifmt,&options);
#endif
//	PrintErrorMsg(n);
	return n == 0;
}

}//namespace device_manager

}//namespace rtplivelib
