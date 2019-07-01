#include "microphonecapture.h"
#include "wasapi.h"
#include "../core/stringformat.h"
#include "../core/logger.h"

namespace rtplivelib {

namespace device_manager {

class MicrophoneCapturePrivateData{
public:
#if defined (WIN64)
	WASAPI audio_api;
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
	d_ptr->audio_api.set_default_device(MicrophoneCapturePrivateData::FT);
	
	auto pair = d_ptr->audio_api.get_current_device_info();
	current_device_name = core::StringFormat::WString2String(pair.second);
	
	/*在子类初始化里开启线程*/
	start_thread();
}

MicrophoneCapture::~MicrophoneCapture() 
{
	exit_thread();
	delete d_ptr;
}

std::map<std::string,std::string> MicrophoneCapture::get_all_device_info() noexcept(false)
{
	std::map<std::string,std::string> info_map;

	auto list_info = d_ptr->audio_api.get_all_device_info(MicrophoneCapturePrivateData::FT);
	for(auto i = list_info.begin(); i != list_info.end(); ++i){
		info_map[core::StringFormat::WString2String(i->second)] = core::StringFormat::WString2String(i->first);
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
	std::lock_guard<std::mutex> lk(d_ptr->fmt_ctx_mutex);
	auto temp = current_device_name;
	current_device_name = name;
	auto result = d_ptr->audio_api.set_current_device(core::StringFormat::String2WString(name),MicrophoneCapturePrivateData::FT);
	if(!result){
		current_device_name = temp;
		core::Logger::Print_APP_Info(core::MessageNum::Device_change_success,
									 "device_manager::MicrophoneCapture::set_current_device_name",
									 LogLevel::ERROR_LEVEL,
									 current_device_name.c_str());
	}
	return result;
}

/**
 * @brief on_start
 * 开始捕捉音频后的回调
 */
AbstractCapture::SharedPacket MicrophoneCapture::on_start() noexcept
{
	std::lock_guard<std::mutex> lk(d_ptr->fmt_ctx_mutex);
	if(d_ptr->audio_api.is_start() == false){
		if( d_ptr->audio_api.start() == false){
			stop_capture();
			return nullptr;
		}
	}
	
	//开始捕捉前，睡眠1ms
	//防止刚解锁就拿到锁，其他线程饥饿
	this->sleep(1);
	
	return d_ptr->audio_api.read_packet();
}

/**
 * @brief on_stop
 * 结束捕捉音频后的回调
 */
void MicrophoneCapture::on_stop() noexcept 
{
	std::lock_guard<std::mutex> lk(d_ptr->fmt_ctx_mutex);
	d_ptr->audio_api.stop();
}

bool MicrophoneCapture::open_device() noexcept
{
	std::lock_guard<std::mutex> lk(d_ptr->fmt_ctx_mutex);
	return d_ptr->audio_api.start();
}

}//namespace device_manager

}//namespace rtplivelib
