#include "audioprocessingfactory.h"

namespace rtplivelib {

namespace device_manager {

/////////////////////////////////////////////////////////////////////////////////////////////

AudioProcessingFactory::AudioProcessingFactory(
		device_manager::MicrophoneCapture *mc,
		device_manager::SoundCardCapture *sc):
	mc_ptr(mc),
	sc_ptr(sc)
{
	start_thread();
}

AudioProcessingFactory::~AudioProcessingFactory()
{
	set_capture(false,false);
	exit_thread();
}

bool AudioProcessingFactory::set_microphone_capture_object(
		device_manager::MicrophoneCapture *mc) noexcept
{
	if(get_thread_pause_condition()){
		mc_ptr = mc;
		return true;
	}
	else
		return false;
}

bool AudioProcessingFactory::set_soundcard_capture_object(
		device_manager::SoundCardCapture *sc) noexcept
{
	if(get_thread_pause_condition()){
		sc_ptr = sc;
		return true;
	}
	else
		return false;
}

void AudioProcessingFactory::set_capture(bool microphone,bool soundcard) noexcept
{
	if(mc_ptr !=nullptr){
		/*如果标志位和原来不一致才会执行下一步*/
		if(microphone != mc_ptr->is_running()){
			/*开启捕捉*/
			if(microphone){
				mc_ptr->start_capture();
			}
			/*关闭*/
			else{
				mc_ptr->stop_capture();
			}
		}
	}
	
	if(sc_ptr != nullptr){
		/*如果标志位和原来不一致才会执行下一步*/
		if(soundcard != sc_ptr->is_running()){
			/*开启捕捉*/
			if(soundcard){
				sc_ptr->start_capture();
			}
			/*关闭*/
			else{
				sc_ptr->stop_capture();
			}
		}
	}
	
	/*唤醒线程*/
	/*这里不需要检查条件，因为在唤醒的时候会自己检查条件*/
	notify_thread();
}

void AudioProcessingFactory::on_thread_run() noexcept
{
	using namespace core;
	
	if(mc_ptr != nullptr&& mc_ptr->is_running() &&
			sc_ptr != nullptr && sc_ptr->is_running()){
		//先不处理双开
	}
	else if( mc_ptr != nullptr&& mc_ptr->is_running() ){
		//只开麦克风
		auto packet = mc_ptr->get_latest();
		if(packet == nullptr)
			return;
		//第一时间回调
		if(GlobalCallBack::Get_CallBack() != nullptr){
			GlobalCallBack::Get_CallBack()->on_microphone_packet(packet.get());
		}
		push_one(packet);
		return;
	}
	else if(sc_ptr != nullptr && sc_ptr->is_running()){
		//只开声卡
		auto packet =sc_ptr->get_latest();
		if(packet == nullptr)
			return;
		
		//裁剪后回调
		if(GlobalCallBack::Get_CallBack() != nullptr){
			GlobalCallBack::Get_CallBack()->on_soundcard_packet(packet.get());
		}
		push_one(packet);
		return;
	}
	else{
		//都不开，直接返回暂停
		return;
	}
}

void AudioProcessingFactory::on_thread_pause() noexcept
{
	
}

bool AudioProcessingFactory::get_thread_pause_condition() noexcept
{
	if(mc_ptr == nullptr && sc_ptr == nullptr){
		return true;
	}
	return !( ( mc_ptr != nullptr && mc_ptr->is_running() ) ||
			  ( sc_ptr != nullptr && sc_ptr->is_running() ) );
}

}// namespace device_manager

} // namespace rtplivelib 

