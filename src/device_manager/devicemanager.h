
#pragma once

#include "videoprocessingfactory.h"
#include "audioprocessingfactory.h"
#include "../player/videoplayer.h"

namespace rtplivelib {

class LiveEngine;

namespace device_manager{

/**
 * @brief The DeviceManager class
 * 该类负责统一所有捕捉类，并且封装一层很简单的接口
 * 具体详细的操作可以通过获取具体对象来处理
 * 也就是很多接口并没有再封装一层，只能通过获取设备对象来处理
 * 
 * 该类实现的接口推荐使用该接口，封装了一层额外做了一些工作，不建议使用对象的接口
 */
class RTPLIVELIBSHARED_EXPORT DeviceManager 
{
public:
	/**
	 * @brief DeviceCapture
	 */
	DeviceManager();
	
	/**
	 * @brief ~DeviceCapture
	 */
	virtual ~DeviceManager();
	
	/**
	 * @brief DeviceCapture
	 * 所有拷贝构造函数都删除了，不存在拷贝
	 */
	DeviceManager(DeviceManager&) = delete;
	DeviceManager(DeviceManager&&) = delete;
	
	/**
	 * @brief setDesktopCaptureEnable
	 * 设置是否捕捉桌面画面
	 * @param enable
	 * true则捕捉桌面画面
	 * 如果没有开启视频捕捉(startVideoCapture),则不捕捉画面，但是标志位不会设置失败
	 * 在开启视频捕捉后依然会捕捉画面
	 * false则不捕捉
	 */
	void set_desktop_capture_enable(bool enable);
	
	/**
	 * @brief setCameraCaptureEnable
	 * 设置是否捕捉摄像头画面
	 * @param enable
	 * true则捕捉摄像头画面
	 * 如果没有开启视频捕捉(startVideoCapture),则不捕捉画面，但是标志位不会设置失败
	 * 在开启视频捕捉后依然会捕捉画面
	 * false则不捕捉
	 */
	void set_camera_capture_enable(bool enable);
	
	/**
	 * @brief setMicrophoneEnable
	 * 设置是否捕捉麦克风音频
	 * @param enable
	 * true则捕捉麦克风音频
	 * 如果没有开启音频捕捉(startAudioCapture),,则不捕捉音频，但是标志位不会设置失败
	 * 在开启音频捕捉后依然会捕捉音频
	 * false则不捕捉
	 */
	void set_microphone_enable(bool enable);
	
	/**
	 * @brief setSoundCardEnable
	 * 设置是否捕捉声卡音频
	 * @param enable
	 * true则捕捉声卡音频
	 * 如果没有开启音频捕捉(startAudioCapture),,则不捕捉音频，但是标志位不会设置失败
	 * 在开启音频捕捉后依然会捕捉音频
	 * false则不捕捉
	 */
	void set_sound_card_enable(bool enable);
	
	/**
	 * @brief desktopCaptureIsOpen
	 * 判断桌面捕捉是否打开
	 * @return
	 * 返回该标志位
	 */
	bool desktop_capture_is_open() noexcept;
	
	/**
	 * @brief cameraCaptureIsOpen
	 * 判断摄像头捕捉是否打开
	 * @return
	 * 返回该标志位
	 */
	bool camera_capture_is_open() noexcept;
	
	/**
	 * @brief micCaptureIsOpen
	 * 判断麦克风捕捉是否打开
	 * @return
	 * 返回该标志位
	 */
	bool mic_capture_is_open() noexcept;
	
	/**
	 * @brief soundCardIsOpen
	 * 判断声卡捕捉是否打开
	 * @return
	 * 返回该标志位
	 */
	bool sound_card_is_open() noexcept;
	
	/**
	 * @brief isCaptureVideo
	 * 判断是否捕捉视频数据
	 * @return
	 * 返回该标志位
	 */
	bool is_capture_video()  noexcept;
	
	/**
	 * @brief isCaptureAudio
	 * 判断是否捕捉音频数据
	 * @return
	 * 返回该标志位
	 */
	bool is_capture_audio() noexcept;
	
	/**
	 * @brief startVideoCapture
	 * 开始捕捉视频画面
	 * 如果没有开启桌面捕捉或者摄像头捕捉，还是不会捕捉画面
	 * 相当于全局视频设置
	 */
	void start_video_capture();
	
	/**
	 * @brief stopVideoCapture
	 * 停止捕捉视频画面
	 * 如果已经开启桌面捕捉或者摄像头捕捉，也会停止捕捉画面
	 * 直到重新调用startVideoCapture打开视频捕捉为止
	 */
	void stop_video_capture();
	
	/**
	 * @brief set_fps
	 * 设置帧数，默认是１５帧
	 */
	void set_fps(int fps);
	
	/**
	 * @brief startAudioCapture
	 * 开始捕捉音频数据
	 * 如果没有开启麦克风或者声卡捕捉，还是不会捕捉音频
	 * 相当于全局音频设置
	 */
	void start_audio_capture();
	
	/**
	 * @brief stopAudioCapture
	 * 停止捕捉音频数据
	 * 如果已经打开任意一个音频捕捉，也会停止该操作
	 * 直到重新调用startAudioCapture打开音频捕捉为止
	 */
	void stop_audio_capture();
	
	/**
	 * @brief get_video_factory
	 * 获取视频加工厂
	 */
	VideoProcessingFactory * get_video_factory() noexcept;
	
	/**
	 * @brief get_audio_factory
	 * 获取音频加工厂
	 */
	AudioProcessingFactory * get_audio_factory() noexcept;
	
	/**
	 * @brief get_desktop_object
	 * 获取桌面捕捉类
	 */
	DesktopCapture * get_desktop_object() noexcept;
	
	/**
	 * @brief get_camera_object
	 * 获取摄像头捕捉类
	 */
	CameraCapture * get_camera_object() noexcept;
	
	/**
	 * @brief get_microphone_object
	 * 获取麦克风捕捉类
	 */
	MicrophoneCapture * get_microphone_object() noexcept;
	
	/**
	 * @brief get_soundcard_object
	 * 获取声卡捕捉类
	 */
	SoundCardCapture * get_soundcard_object() noexcept;
	
private:
	DesktopCapture				desktop_capture;
	CameraCapture				camera_capture;
	MicrophoneCapture			mic_capture;
	SoundCardCapture			soundcard_capture;
	VideoProcessingFactory		video_factory;
	AudioProcessingFactory		audio_factory;
	bool						desktop_open_flag;
	bool						camera_open_flag;
	bool						mic_open_flag;
	bool						sc_open_flag;
	bool						video_open_flag;
	bool						audio_open_flag;
	
	core::TaskQueue				camera_output_queue;
	core::TaskQueue				desktop_output_queue;
	core::TaskQueue				mic_output_queue;
	core::TaskQueue				soundcard_output_queue;
	player::VideoPlayer			video_player;
	player::AudioPlayer			audio_player;
	core::TaskQueue				video_player_queue;
	core::TaskQueue				audio_player_queue;
	
	core::TaskQueue				video_factory_output_queue;
	core::TaskQueue				audio_factory_output_queue;
	
	friend class rtplivelib::LiveEngine;
};

inline bool DeviceManager::desktop_capture_is_open() noexcept					{	return desktop_open_flag;		}
inline bool DeviceManager::camera_capture_is_open() noexcept					{	return camera_open_flag;		}
inline bool DeviceManager::mic_capture_is_open() noexcept						{	return mic_open_flag;			}
inline bool DeviceManager::sound_card_is_open() noexcept						{	return sc_open_flag;			}
inline bool DeviceManager::is_capture_video() noexcept							{	return video_open_flag;			}
inline bool DeviceManager::is_capture_audio() noexcept							{	return audio_open_flag;			}
inline VideoProcessingFactory * DeviceManager::get_video_factory() noexcept		{	return &video_factory;			}
inline AudioProcessingFactory * DeviceManager::get_audio_factory() noexcept		{	return &audio_factory;			}
inline DesktopCapture * DeviceManager::get_desktop_object() noexcept			{	return &desktop_capture;		}
inline CameraCapture * DeviceManager::get_camera_object() noexcept				{	return &camera_capture;			}
inline MicrophoneCapture * DeviceManager::get_microphone_object() noexcept		{	return &mic_capture;			}
inline SoundCardCapture * DeviceManager::get_soundcard_object() noexcept		{	return &soundcard_capture;		}

} // namespace device_manager

} //namespace rtplivelib


