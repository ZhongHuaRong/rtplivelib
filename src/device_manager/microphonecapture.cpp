#include "microphonecapture.h"
#include "../core/stringformat.h"
#include "../core/logger.h"
#if defined (WIN64)
#include "wasapi.h"
#endif

namespace rtplivelib {

namespace device_manager {

class MicrophoneCapturePrivateData{
public:
#if defined (WIN64)
	WASAPI audio_api;
	static constexpr WASAPI::FlowType FT{WASAPI::CAPTURE};
#endif
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
	
	auto result = d_ptr->audio_api.set_current_device(core::StringFormat::String2WString(name),MicrophoneCapturePrivateData::FT);
    constexpr char api[] = "device_manager::MicrophoneCapture::set_current_device_name";
    if(result){
        current_device_name = name;
		core::Logger::Print_APP_Info(core::MessageNum::Device_change_success,
                                     api,
                                     LogLevel::INFO_LEVEL,
									 current_device_name.c_str());
    } else{
        core::Logger::Print_APP_Info(core::MessageNum::Device_change_failed,
                                     api,
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
	if(d_ptr->audio_api.is_start() == false){
		if( d_ptr->audio_api.start() == false){
			stop_capture();
			return nullptr;
		}
	}
	
	return d_ptr->audio_api.read_packet();
}

/**
 * @brief on_stop
 * 结束捕捉音频后的回调
 */
void MicrophoneCapture::on_stop() noexcept 
{
	d_ptr->audio_api.stop();
}

bool MicrophoneCapture::open_device() noexcept
{
	return d_ptr->audio_api.start();
}

}//namespace device_manager

}//namespace rtplivelib
