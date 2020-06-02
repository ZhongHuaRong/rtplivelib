
#include "devicemanager.h"
#include "../core/logger.h"

namespace rtplivelib {

namespace device_manager {

DeviceManager::DeviceManager() :
	desktop_open_flag(false),
	camera_open_flag(false),
	mic_open_flag(false),
	sc_open_flag(false),
	audio_open_flag(false)
{
	desktop_capture.add_output_queue(&desktop_output_queue);
	camera_capture.add_output_queue(&camera_output_queue);
	mic_capture.add_output_queue(&mic_output_queue);
	soundcard_capture.add_output_queue(&soundcard_output_queue);
	
	video_factory.add_output_queue(&video_factory_output_queue);
	video_factory.add_output_queue(&video_player_queue);
	
	audio_factory.add_output_queue(&audio_factory_output_queue);
	audio_factory.add_output_queue(&audio_player_queue);
	
//	video_factory.add_input_queue(&camera_output_queue);
//	video_factory.add_input_queue(&desktop_output_queue);
//	audio_factory.add_input_queue(&mic_output_queue);
//	audio_factory.add_input_queue(&soundcard_output_queue);
	set_fps(15);
}

DeviceManager::~DeviceManager() {
//	video_factory.clear_input_queue();
//	video_factory.clear_output_queue();
//	video_factory.notify_thread();
//	audio_factory.clear_input_queue();
//	audio_factory.clear_output_queue();
//	audio_factory.notify_thread();
}

/**
 * @brief setDesktopCaptureEnable
 * 设置是否捕捉桌面画面
 * @param enable
 * true则捕捉桌面画面
 * 如果没有开启视频捕捉(startVideoCapture),则不捕捉画面，但是标志位不会设置失败
 * 在开启视频捕捉后依然会捕捉画面
 * false则不捕捉
 */
void DeviceManager::set_desktop_capture_enable(bool enable) {
	desktop_open_flag = enable;
	if (enable){
		desktop_capture.start_capture(is_capture_video());
		video_factory.add_input_queue(&desktop_output_queue);
		
	}
	else {
		desktop_capture.stop_capture();
		video_factory.remove_input_queue(&desktop_output_queue);
	}
	video_factory.start_thread();
}

/**
 * @brief setCameraCaptureEnable
 * 设置是否捕捉摄像头画面
 * @param enable
 * true则捕捉摄像头画面
 * 如果没有开启视频捕捉(startVideoCapture),则不捕捉画面，但是标志位不会设置失败
 * 在开启视频捕捉后依然会捕捉画面
 * false则不捕捉
 */
void DeviceManager::set_camera_capture_enable(bool enable) {
	camera_open_flag = enable;
	if (enable) {
		camera_capture.start_capture(is_capture_video());
		video_factory.add_input_queue(&camera_output_queue);
	}
	else {
		camera_capture.stop_capture();
		video_factory.remove_input_queue(&camera_output_queue);
	}
	video_factory.start_thread();
}

/**
 * @brief setMicrophoneEnable
 * 设置是否捕捉麦克风音频
 * @param enable
 * true则捕捉麦克风音频
 * 如果没有开启音频捕捉(startAudioCapture),,则不捕捉音频，但是标志位不会设置失败
 * 在开启音频捕捉后依然会捕捉音频
 * false则不捕捉
 */
void DeviceManager::set_microphone_enable(bool enable) {
	mic_open_flag = enable;
	if (enable)
		mic_capture.start_capture(is_capture_audio());
	else
		mic_capture.stop_capture();
	audio_factory.notify_thread();
}

/**
 * @brief setSoundCardEnable
 * 设置是否捕捉声卡音频
 * @param enable
 * true则捕捉声卡音频
 * 如果没有开启音频捕捉(startAudioCapture),,则不捕捉音频，但是标志位不会设置失败
 * 在开启音频捕捉后依然会捕捉音频
 * false则不捕捉
 */
void DeviceManager::set_sound_card_enable(bool enable) {
	sc_open_flag = enable;
	if (enable)
		soundcard_capture.start_capture(is_capture_audio());
	else
		soundcard_capture.stop_capture();
	audio_factory.notify_thread();
}

/**
 * @brief startVideoCapture
 * 开始捕捉视频画面
 * 如果没有开启桌面捕捉或者摄像头捕捉，还是不会捕捉画面
 * 相当于全局视频设置
 */
void DeviceManager::start_video_capture() {
	video_open_flag = true;
	set_camera_capture_enable(camera_open_flag);
	set_desktop_capture_enable(desktop_open_flag);
}

/**
 * @brief stopVideoCapture
 * 停止捕捉视频画面
 * 如果已经开启桌面捕捉或者摄像头捕捉，也会停止捕捉画面
 * 直到重新调用startVideoCapture打开视频捕捉为止
 */
void DeviceManager::stop_video_capture() {
	video_open_flag = false;
	//因为set接口重置了flag，所以先记录下来
	auto camera_flag = camera_open_flag;
	auto desktop_flag = desktop_open_flag;
	set_camera_capture_enable(false);
	set_desktop_capture_enable(false);
	desktop_open_flag = camera_flag;
	desktop_open_flag = desktop_flag;
}

/**
 * @brief set_fps
 * 设置帧数，默认是15帧
 */
void DeviceManager::set_fps(int fps)
{
	camera_capture.set_fps(fps);
	desktop_capture.set_fps(fps);
}

/**
 * @brief startAudioCapture
 * 开始捕捉音频数据
 * 如果没有开启麦克风或者声卡捕捉，还是不会捕捉音频
 * 相当于全局音频设置
 */
void DeviceManager::start_audio_capture() {
	audio_open_flag = true;
	audio_factory.set_capture(mic_open_flag,sc_open_flag);
}

/**
 * @brief stopAudioCapture
 * 停止捕捉音频数据
 * 如果已经打开任意一个音频捕捉，也会停止该操作
 * 直到重新调用startAudioCapture打开音频捕捉为止
 */
void DeviceManager::stop_audio_capture() {
	audio_open_flag = false;
	audio_factory.set_capture(false,false);
	
}

}// namespace device_manager

}// namespace rtplivelib
