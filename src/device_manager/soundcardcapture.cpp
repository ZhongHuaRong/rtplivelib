#include "soundcardcapture.h"
#include "wasapi.h"
#include "../core/stringformat.h"

namespace rtplivelib {

namespace device_manager {

struct SoundCardCapturePrivateData{
public:
#if defined (WIN64)
	WASAPI audio_api;
	static constexpr WASAPI::FlowType FT{WASAPI::RENDER};
	HANDLE event{nullptr};
#endif
	std::mutex fmt_ctx_mutex;
	std::string fmt_name;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

SoundCardCapture::SoundCardCapture() :
	AbstractCapture(CaptureType::Soundcard),
	d_ptr(new SoundCardCapturePrivateData)
{
#if defined (WIN64)
#if _WIN32_WINNT >= 0x0600
	d_ptr->event = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
#else
	d_ptr->event = CreateEvent(nullptr, false, false, nullptr);
#endif
#endif
	
	d_ptr->audio_api.set_default_device(SoundCardCapturePrivateData::FT);
	
	auto pair = d_ptr->audio_api.get_current_device_info();
	current_device_name = core::StringFormat::WString2String(pair.second);
	
	/*在子类初始化里开启线程*/
	start_thread();
}

SoundCardCapture::~SoundCardCapture() 
{
	exit_thread();
#if defined (WIN64)
	if (d_ptr->event)
		CloseHandle(d_ptr->event);
#endif
	delete d_ptr;
}

std::map<std::string, std::string> SoundCardCapture::get_all_device_info() noexcept
{
	std::map<std::string,std::string> info_map;

	auto list_info = d_ptr->audio_api.get_all_device_info(SoundCardCapturePrivateData::FT);
	for(auto i = list_info.begin(); i != list_info.end(); ++i){
		info_map[core::StringFormat::WString2String(i->second)] = core::StringFormat::WString2String(i->first);
	}
	return info_map;
}

bool SoundCardCapture::set_current_device_name(std::string name) noexcept
{
	if(name.compare(current_device_name) == 0)
		return true;
	std::lock_guard<std::mutex> lk(d_ptr->fmt_ctx_mutex);
	auto temp = current_device_name;
	current_device_name = name;
	auto result = d_ptr->audio_api.set_current_device(core::StringFormat::String2WString(name),SoundCardCapturePrivateData::FT);
	if(!result){
		current_device_name = temp;
	}
	return result;
}

/**
 * @brief OnStart
 * 开始捕捉音频后的回调
 */
AbstractCapture::SharedPacket SoundCardCapture::on_start()  noexcept
{
	std::lock_guard<std::mutex> lk(d_ptr->fmt_ctx_mutex);
	WaitForSingleObject(d_ptr->event, 5);
	
	auto packet = d_ptr->audio_api.get_packet();

	AbstractCapture::SharedPacket p(packet);
	return p;
}

/**
 * @brief OnStart
 * 结束捕捉音频后的回调
 */
void SoundCardCapture::on_stop() noexcept
{
    /*并没有任何操作，将标志位isrunning设为false就可以暂停了*/
    std::lock_guard<std::mutex> lk(d_ptr->fmt_ctx_mutex);
	d_ptr->audio_api.stop();
}

}//namespace device_manager

}//namespace rtplivelib
