
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
	video_factory.set_camera_capture_object(&camera_capture);
	video_factory.set_desktop_capture_object(&desktop_capture);
	audio_factory.set_microphone_capture_object(&mic_capture);
	audio_factory.set_soundcard_capture_object(&soundcard_capture);
	set_fps(15);
}

DeviceManager::~DeviceManager() {
	//析构的时候全部东西都要手动释放
	video_factory.set_capture(false,false);
	video_factory.set_camera_capture_object(nullptr);
	video_factory.set_desktop_capture_object(nullptr);
	audio_factory.set_capture(false,false);
	audio_factory.set_microphone_capture_object(nullptr);
	audio_factory.set_soundcard_capture_object(nullptr);
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
	if (enable)
		desktop_capture.start_capture(is_capture_video());
	else
		desktop_capture.stop_capture();
	video_factory.notify_capture();
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
	if (enable)
		camera_capture.start_capture(is_capture_video());
	else
		camera_capture.stop_capture();
	video_factory.notify_capture();
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
	audio_factory.notify_capture();
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
	audio_factory.notify_capture();
}

/**
 * @brief startVideoCapture
 * 开始捕捉视频画面
 * 如果没有开启桌面捕捉或者摄像头捕捉，还是不会捕捉画面
 * 相当于全局视频设置
 */
void DeviceManager::start_video_capture() {
    video_open_flag = true;
	video_factory.set_capture(camera_open_flag,desktop_open_flag);
}

/**
 * @brief stopVideoCapture
 * 停止捕捉视频画面
 * 如果已经开启桌面捕捉或者摄像头捕捉，也会停止捕捉画面
 * 直到重新调用startVideoCapture打开视频捕捉为止
 */
void DeviceManager::stop_video_capture() {
    video_open_flag = false;
	video_factory.set_capture(false,false);
}

/**
 * @brief set_fps
 * 设置帧数，默认是１５帧
 */
void DeviceManager::set_fps(int fps)
{
	video_factory.set_fps(fps);
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
