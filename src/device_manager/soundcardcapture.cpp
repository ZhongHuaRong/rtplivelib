#include "soundcardcapture.h"
#include "../core/stringformat.h"
#include "../core/logger.h"
#if defined (WIN64)
#include "wasapi.h"
#endif
#if defined (unix)
#include "alsa.h"
#endif

namespace rtplivelib {

namespace device_manager {

struct SoundCardCapturePrivateData{
public:
#if defined (WIN64)
	WASAPI audio_api;
	static constexpr WASAPI::FlowType FT{WASAPI::RENDER};
#elif defined (unix)
	ALSA audio_api;
	static constexpr ALSA::FlowType FT{ALSA::RENDER};
#endif
};

////////////////////////////////////////////////////////////////////////////////////////////////////

SoundCardCapture::SoundCardCapture() :
	AbstractCapture(CaptureType::Soundcard),
	d_ptr(new SoundCardCapturePrivateData)
{
	d_ptr->audio_api.set_default_device(SoundCardCapturePrivateData::FT);
	
	auto pair = d_ptr->audio_api.get_current_device_info();
#if defined (WIN64)
	current_device_info.first = core::StringFormat::WString2String(pair.first);
	current_device_info.second = core::StringFormat::WString2String(pair.second);
#elif defined(unix)
	current_device_info.first = pair.first;
	current_device_info.second = pair.second;
#endif
	
	/*在子类初始化里开启线程*/
	start_thread();
}

SoundCardCapture::~SoundCardCapture() 
{
	d_ptr->audio_api.stop();
	exit_thread();
	delete d_ptr;
}

std::map<std::string, std::string> SoundCardCapture::get_all_device_info()  noexcept(false)
{
	std::map<std::string,std::string> info_map;
	
	auto list_info = d_ptr->audio_api.get_all_device_info(SoundCardCapturePrivateData::FT);
	for(auto i = list_info.begin(); i != list_info.end(); ++i){
#if defined (WIN64)
		info_map[core::StringFormat::WString2String(i->first)] = core::StringFormat::WString2String(i->second);
#elif defined(unix)
		info_map[i->first] = i->second;
#endif
	}
	return info_map;
}

bool SoundCardCapture::set_current_device(std::string device_id) noexcept
{
	if(device_id.compare(current_device_info.first) == 0)
		return true;
#if defined (WIN64)
	auto result = d_ptr->audio_api.set_current_device(core::StringFormat::String2WString(device_id),
													  SoundCardCapturePrivateData::FT);
#elif defined(unix)
	auto result = d_ptr->audio_api.set_current_device(device_id,
													  SoundCardCapturePrivateData::FT);
#endif
	if(result){
		auto pair = d_ptr->audio_api.get_current_device_info();
#if defined (WIN64)
		current_device_info.first = core::StringFormat::WString2String(pair.first);
		current_device_info.second = core::StringFormat::WString2String(pair.second);
#elif defined(unix)
		current_device_info.first = pair.first;
		current_device_info.second = pair.second;
#endif
		core::Logger::Print_APP_Info(core::Result::Device_change_success,
									 __PRETTY_FUNCTION__,
									 LogLevel::INFO_LEVEL,
									 current_device_info.second.c_str());
	} else{
		core::Logger::Print_APP_Info(core::Result::Device_change_failed,
									 __PRETTY_FUNCTION__,
									 LogLevel::ERROR_LEVEL,
									 current_device_info.second.c_str());
	}
	return result;
}

bool SoundCardCapture::set_default_device() noexcept
{
	return d_ptr->audio_api.set_default_device(SoundCardCapturePrivateData::FT);
}

/**
 * @brief OnStart
 * 开始捕捉音频后的回调
 */
AbstractCapture::SharedPacket SoundCardCapture::on_start()  noexcept
{
	if(d_ptr->audio_api.is_start() == false){
		if( open_device() == false){
			stop_capture();
			return nullptr;
		}
	}
	
	return d_ptr->audio_api.read_packet();
}

/**
 * @brief OnStart
 * 结束捕捉音频后的回调
 */
void SoundCardCapture::on_stop() noexcept
{
	d_ptr->audio_api.stop();
}

bool SoundCardCapture::open_device() noexcept
{
	return d_ptr->audio_api.start();
}

}//namespace device_manager

}//namespace rtplivelib
