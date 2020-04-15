
#pragma once

#include "../core/abstractthread.h"
#include "../core/abstractqueue.h"
#include "rtpsession.h"
#include "../core/format.h"

namespace rtplivelib{

namespace rtp_network{

class RtpSendThreadPrivateData;

/**
 * @brief The RTPSendThread class
 * 该类是用来开线程发送rtp数据包
 * 该类对session没有删除所有权
 * 由于session所有操作都不是线程安全的
 * 所以在设置参数的接口上，使用该类的接口
 */
class RTPLIVELIBSHARED_EXPORT RTPSendThread : public core::AbstractThread
{
private:
	using SendQueue = core::AbstractQueue<core::FramePacket>;
public:
	RTPSendThread();

	/**
	 * @brief ~RtpSendThread
	 * 优雅的退出线程
	 */
	virtual ~RTPSendThread() override;
		
	/**
	 * @brief set_send_queue
	 * 设置需要发送的视频队列，一般是videoencoder
	 * @param input_queue
	 * 视频发送队列
	 */
	void set_video_send_queue(SendQueue * input_queue) noexcept;
	
	/**
	 * @brief set_audio_send_queue
	 * 设置需要发送的音频队列，一般是audioencoder
	 * @param input_queue
	 * 音频发送队列
	 */
	void set_audio_send_queue(SendQueue * input_queue) noexcept;
	
	/**
	 * @brief set_video_session
	 * 设置一个视频rtp会话，用于发包,如果不设置则视频信息无法发送
	 * 该类不拥有该video_session对象的所有权
	 * 需要先设置为空，然后在外部析构该对象
	 * 或者先析构本类对象再在外部析构该对象
	 */
	void set_video_session(RTPSession *video_session) noexcept;
	
	/**
	 * @brief set_audio_session
	 * 设置一个音频rtp会话，用于发包,如果不设置则音频信息无法发送
	 * 该类不拥有该audio_session对象的所有权
	 * 需要先设置为空，然后在外部析构该对象
	 * 或者先析构本类对象再在外部析构该对象
	 */
	void set_audio_session(RTPSession *audio_session) noexcept;
	
	/**
	 * @brief send_video_packet
	 * 利用之前设置的RTPSession，发送视频编码帧
	 * @param packet
	 * 视频包
	 */
	void send_video_packet(core::FramePacket::SharedPacket packet);
	
	/**
	 * @brief send_audio_packet
	 * 利用之前设置的RTPSession，发送音频编码帧
	 * @param packet
	 * 音频包
	 */
	void send_audio_packet(core::FramePacket::SharedPacket packet);
	
	/**
	 * @brief set_destination
	 * 设置目标信息
	 * @param ip
	 * 目标ＩＰ
	 * @param port_base
	 * 目标端口，将会占用４个
	 * x,x+1是视频端口
	 * x+2,x+3是音频端口
	 * 默认是0，由系统自动分配
	 */
	void set_destination(const uint8_t *ip,uint16_t port_base = 0) noexcept;
	
	/**
	 * @brief set_local_name
	 * 设置在会话中的名字
	 * 只有当音视频都设置成功才返回true
	 * 否则都是返回false
	 * 如果有一个设置失败，不会将另一个设置成功的重置
	 * 
	 * 由于rtp协议问题，这个设置需要一段时间其他用户才可以看到
	 */
	bool set_local_name(const std::string& name) noexcept;
	
	/**
	 * @brief set_room_name
	 * 设置房间名字
	 * 只有当音视频都设置成功才返回true
	 * 否则都是返回false
	 * 如果有一个设置失败，不会将另一个设置成功的重置
	 * 
	 * 当房间名字设置成功后，会自动启动会话，收发信息(如果开启音视频捕捉才会发送)
	 * 当把房间名字设为空时，则会自动退出会话
	 * 按照正常逻辑来说，换房间一般需要先设置为空退出当前房间，然后在设置新的房间名
	 * 
	 * 由于rtp协议问题，这个设置需要一段时间其他用户才可以看到
	 */
	bool set_room_name(const std::string& name) noexcept;
	
	/**
	 * @brief set_push_flag
	 * 设置推流标志位
	 * 如果为true则允许推流
	 * 否则不允许推流
	 * 默认不允许
	 */
	bool set_push_flag(bool flag) noexcept;
protected:
	/**
	 * @brief on_thread_run
	 * 线程运行时需要处理的操作
	 */
	virtual void on_thread_run() noexcept override;
	
	/**
	 * @brief get_thread_pause_condition
	 * 该函数用于判断线程是否需要暂停
	 * @return 
	 * 返回true则线程睡眠
	 * 如果发送队列和rtp会话有一个参数是空则睡眠
	 */
	virtual bool get_thread_pause_condition() noexcept override;
	
private:
	SendQueue						*_video_queue;
	SendQueue						*_audio_queue;
	RTPSession						*_video_session;
	RTPSession						*_audio_session;
	std::recursive_mutex			_mutex;
	RtpSendThreadPrivateData * const d_ptr;
	
	friend class RtpSendThreadPrivateData;
};

inline void RTPSendThread::set_video_send_queue(SendQueue * input_queue) noexcept			{
	std::lock_guard<decltype(_mutex)> lk(_mutex);
	_video_queue = input_queue;
	this->notify_thread();
}
inline void RTPSendThread::set_audio_send_queue(SendQueue * input_queue) noexcept			{
	std::lock_guard<decltype(_mutex)> lk(_mutex);
	_audio_queue = input_queue;
	this->notify_thread();
}
inline bool RTPSendThread::get_thread_pause_condition() noexcept							{
	//只要有一组是正常的，线程就不需要暂停
	return ( _video_queue == nullptr || _video_session == nullptr) &&
		   ( _audio_queue == nullptr || _audio_session == nullptr);
}

} // namespace rtp_network

} // namespace rtplivelib
