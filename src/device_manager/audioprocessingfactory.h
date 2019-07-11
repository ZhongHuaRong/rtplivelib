
#pragma once

#include "abstractcapture.h"
#include "microphonecapture.h"
#include "soundcardcapture.h"
#include "../player/audioplayer.h"
#include "../core/globalcallback.h"

class AVInputFormat;
class AVFormatContext;

namespace rtplivelib {

namespace device_manager {

/**
 * @brief The AudioCapture class
 * 该类统一音频处理
 * 将会把采集到的音频重采样(统一采样格式)，然后编码，格式为AAC
 * 该类也会提供原始音频数据获取接口，让外部调用可以做数据前处理
 */
class RTPLIVELIBSHARED_EXPORT AudioProcessingFactory:
        public core::AbstractQueue<core::FramePacket::SharedPacket,core::FramePacket::SharedPacket,core::NotDelete>
{
public:
	/**
	 * @brief VideoProcessingFactory
	 * 该类需要传入CameraCapture和DesktopCapture子类的实例
	 * 该类将会统一两个对象的所有操作，方便管理
	 * 传入对象后不建议在外部使用这两个对象
	 */
	AudioProcessingFactory(device_manager::MicrophoneCapture * mc = nullptr,
						   device_manager::SoundCardCapture * sc = nullptr);
	
	virtual ~AudioProcessingFactory() override;
	
	/**
	 * @brief set_camera_capture_object
	 * 给你第二次机会设置对象指针
	 * @return
	 * 如果在运行过程中设置，则会返回false
	 * 设置同步机制会浪费资源，所以只要发现是线程运行过程中一律返回false
	 */
	bool set_microphone_capture_object(device_manager::MicrophoneCapture * mc) noexcept;
	
	/**
	 * @brief set_desktop_capture_object
	 * 给你第二次机会设置对象指针
	 * @return
	 * 如果在运行过程中设置，则会返回false
	 * 设置同步机制会浪费资源，所以只要发现是线程运行过程中一律返回false
	 */
	bool set_soundcard_capture_object(device_manager::SoundCardCapture * sc) noexcept;
	
	/**
	 * @brief set_capture
	 * 设置是否捕捉数据
	 * 如果没有实例，设置什么都没有作用
	 * 两个同时为false则处理完所有剩余数据后暂停线程
	 * 所以不要太快开启捕捉
	 * @param microphone
	 * true:捕捉麦克风数据
	 * false:不捕捉
	 * @param soundcard
	 * true:捕捉声卡数据
	 * false:不捕捉
	 */
	void set_capture(bool microphone,bool soundcard) noexcept;

	/**
	 * @brief notify_capture
	 * 外部调用start_capture接口捕捉的，需要调用此函数
	 */
	void notify_capture() noexcept;

    /**
     * @brief play_microphone_audio
     * 播放本地麦克风音频
     * @param flag
     * true:播放
     * false:暂停
     */
    void play_microphone_audio(bool flag) noexcept;
	
	/**
	 * 这里提供接口获取底层对象，直接使用对象的接口更加方便的获取各种参数
	 */
	device_manager::MicrophoneCapture * get_microphone_capture_object() noexcept;
	device_manager::SoundCardCapture * get_soundcard_capture_object() noexcept;
	
protected:
	
	/**
	 * @brief on_thread_run
	 * 线程运行时需要处理的操作
	 */
	virtual void on_thread_run() noexcept override;
	
	/**
	 * @brief on_thread_pause
	 * 线程暂停时的回调
	 * 这个回调可能需要耗费一点时间，需要处理队列剩余数据
	 * 所以暂停后，不要马上开始
	 */
	virtual void on_thread_pause() noexcept override;
	
	/**
	 * @brief get_thread_pause_condition
	 * 该函数用于判断线程是否需要暂停
	 * 线程不需要运行时需要让这个函数返回true
	 * 如果需要重新唤醒线程，则需要让该函数返回false并
	 * 调用notify_thread唤醒(顺序不要反了)
	 * @return 
	 * 返回true则线程睡眠
	 * 默认是返回true，线程启动即睡眠
	 */
	virtual bool get_thread_pause_condition() noexcept override;
private:
	device_manager::MicrophoneCapture *mc_ptr{nullptr};
	device_manager::SoundCardCapture *sc_ptr{nullptr};
    //用时初始化
    player::AudioPlayer * player{nullptr};
    volatile bool play_flag{false};
};

inline device_manager::MicrophoneCapture *AudioProcessingFactory::get_microphone_capture_object() noexcept		{		return mc_ptr;}
inline device_manager::SoundCardCapture *AudioProcessingFactory::get_soundcard_capture_object() noexcept		{		return sc_ptr;}
inline void AudioProcessingFactory::notify_capture() noexcept													{		this->notify_thread();}

}// namespace device_manager

} // namespace rtplivelib

