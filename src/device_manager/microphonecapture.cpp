#include "microphonecapture.h"
#include "wasapi.h"
#include "../core/stringformat.h"

namespace rtplivelib {

namespace device_manager {

class MicrophoneCapturePrivateData{
public:
#if defined (WIN64)
	WASAPI audio_api;
	static constexpr WASAPI::FlowType FT{WASAPI::CAPTURE};
	HANDLE event{nullptr};
#endif
	std::mutex fmt_ctx_mutex;
	std::string fmt_name;
};

//////////////////////////////////////////////////////////////////////////////////////////

MicrophoneCapture::MicrophoneCapture() :
	AbstractCapture(AbstractCapture::CaptureType::Microphone),
	d_ptr(new MicrophoneCapturePrivateData)
{
#if _WIN32_WINNT >= 0x0600
	d_ptr->event = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
#else
	d_ptr->event = CreateEvent(nullptr, false, false, nullptr);
#endif
	
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
	WaitForSingleObject(d_ptr->event, 5);
	
	auto packet = d_ptr->audio_api.get_packet();

	AbstractCapture::SharedPacket p(packet);
	return p;
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
	return d_ptr->audio_api.start(d_ptr->event);
}

}//namespace device_manager

}//namespace rtplivelib
