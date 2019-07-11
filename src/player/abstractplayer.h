#pragma once
#include "../device_manager/abstractcapture.h"
#include <mutex>

namespace rtplivelib{

namespace player {

enum struct PlayFormat{
	PF_VIDEO = 0x01,
	PF_AUDIO
};

/**
 * @brief The AbstractPlayer class
 * 用于播放视频或者音频的抽象基类
 * 使用SDL实现
 * 初始化和释放操作
 */
class RTPLIVELIBSHARED_EXPORT AbstractPlayer:
		public core::AbstractThread
{
public:
	explicit AbstractPlayer(PlayFormat format);
	
	virtual ~AbstractPlayer() override;
	
	/**
	 * @brief get_init_result
	 * 获取初始化结果，如果初始化失败则无法播放
	 * 返回0则说明初始化成功
	 */
	bool get_init_result() noexcept;
	
	/**
	 * @brief setPlayerObject
	 * 简易播放接口，可以传入捕捉类的子类实例，然后调用start_capture接口开始捕捉数据
	 * 则可以显示画面或者播放音频
	 * (该接口不提供数据前处理，需要前处理可以继承捕捉类实现on_frame_data,而且需要该接口返回true)
	 * bug:使用该接口播放视频时，如果拉伸窗口则会使画面停止
	 * @param object
	 * 对象实例
	 * 传入nullptr则停止播放，关闭内部线程
	 * @param winId
	 * 窗口显示ID，音频则无视该参数
	 */
	void set_player_object(device_manager::AbstractCapture * object,
							void * winId = nullptr) noexcept;
	
	/**
	 * @brief set_win_id
	 * 设置窗口id,音频下该接口没作用
	 * @param id
	 * 窗口id
	 * @param format
	 * 显示的图像格式
	 */
	virtual void set_win_id(void *id) noexcept;
	
	/**
	 * @brief show
	 * 重载函数
	 */
	virtual bool play(core::FramePacket::SharedPacket packet) noexcept;
	
	/**
	 * @brief show
	 * 子类实现显示方案
	 */
	virtual bool play(const core::Format& format,uint8_t * data[],int linesize[]) noexcept  = 0;
protected:
	/**
	 * @brief on_thread_run
	 * 重写
	 */
	virtual void on_thread_run() noexcept override;

	/**
	 * @brief on_thread_run
	 * 重写
	 * 默认返回false，线程不存在暂停(因为是简易接口，所以不希望外部使用notify唤醒)
	 * 一般在等待资源时卡住线程
	 * 线程在设置完object后才开启
	 */
	virtual bool get_thread_pause_condition() noexcept override;
private:
	/**
	 * @brief _get_next_packet
	 * 从队列里获取下一个包
	 * @return 
	 */
	core::FramePacket::SharedPacket _get_next_packet() noexcept;
protected:
	device_manager::AbstractCapture * _play_object;
private:
	int _init_result;
	PlayFormat _fmt;
	std::mutex _object_mutex;
};

inline bool AbstractPlayer::get_init_result() noexcept									{		return _init_result;}
inline bool AbstractPlayer::get_thread_pause_condition() noexcept						{		return false;}
inline void AbstractPlayer::set_win_id(void *) noexcept									{}
inline bool AbstractPlayer::play(core::FramePacket::SharedPacket packet)	noexcept	{
	return play(packet->format,packet->data,packet->linesize);
}

}// namespace display

} // namespace rtplivelib
