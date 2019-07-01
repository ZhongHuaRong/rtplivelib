#include "soundcardcapture.h"
#include "wasapi.h"
#include "../core/stringformat.h"
#include "../core/logger.h"

namespace rtplivelib {

namespace device_manager {

struct SoundCardCapturePrivateData{
public:
#if defined (WIN64)
	WASAPI audio_api;
	static constexpr WASAPI::FlowType FT{WASAPI::RENDER};
#endif
	std::string fmt_name;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

SoundCardCapture::SoundCardCapture() :
	AbstractCapture(CaptureType::Soundcard),
	d_ptr(new SoundCardCapturePrivateData)
{
	
	d_ptr->audio_api.set_default_device(SoundCardCapturePrivateData::FT);
	
	auto pair = d_ptr->audio_api.get_current_device_info();
	current_device_name = core::StringFormat::WString2String(pair.second);
	
	/*在子类初始化里开启线程*/
	start_thread();
}

SoundCardCapture::~SoundCardCapture() 
{
	d_ptr->audio_api.stop();
	exit_thread();
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
	auto temp = current_device_name;
	current_device_name = name;
	auto result = d_ptr->audio_api.set_current_device(core::StringFormat::String2WString(name),SoundCardCapturePrivateData::FT);
	if(!result){
		current_device_name = temp;
		core::Logger::Print_APP_Info(core::MessageNum::Device_change_success,
									 "device_manager::SoundCardCapture::set_current_device_name",
									 LogLevel::ERROR_LEVEL,
									 current_device_name.c_str());
	}
	return result;
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
