
#pragma once

#include "../core/abstractqueue.h"
#include "../core/format.h"

namespace rtplivelib {

namespace codec {

class AudioEncoderPrivateData;

/**
 * @brief The VideoEncode class
 * 编码默认采用的是AAC,也只有AAC
 * 先不提供更改编码器的接口
 */
class RTPLIVELIBSHARED_EXPORT AudioEncoder : 
		public core::AbstractQueue<core::FramePacket::SharedPacket,core::FramePacket::SharedPacket,core::NotDelete>
{
public:
	using Queue = core::AbstractQueue<core::FramePacket::SharedPacket,core::FramePacket::SharedPacket,core::NotDelete>;
public:
	/**
	 * @brief AudioEncoder
	 * 不带参数的构造函数，没啥作用，需要调用set_input_queue设置一个输入队列
	 * 简称生产者
	 */
	AudioEncoder();
	
	/**
	 * @brief VideoEncode
	 * 带有输入队列参数的构造函数，将会直接工作
	 * @param input_queue
	 * 输入队列
	 */
	explicit AudioEncoder(Queue * input_queue);
	
	/**
	 * @brief ~VideoEncoder
	 * 释放资源
	 */
	virtual ~AudioEncoder() override;
	
	/**
	 * @brief get_codec_id
	 * 获取当前编码器id
	 * 给外部提供当前是哪种编码格式
	 * @return 
	 */
	int get_encoder_id() noexcept;
	
	/**
	 * @brief set_input_queue
	 * 设置一个队列(生产者)
	 * 只有设置了队列，线程才启动
	 */
	void set_input_queue(Queue * input_queue) noexcept;
	
	/**
	 * @brief has_input_queue
	 * 判断是否含有队列
	 */
	bool has_input_queue() noexcept;
	
	/**
	 * @brief get_input_queue
	 * 获取内置的队列，可能为nullptr
	 */
	const Queue * get_input_queue() noexcept;
protected:
	/**
	 * @brief on_thread_run
	 * 线程运行时需要处理的操作
	 */
	virtual void on_thread_run() noexcept override;
	
	/**
	 * @brief on_thread_pause
	 * 线程暂停时的回调
	 */
	virtual void on_thread_pause() noexcept override;
	
	/**
	 * @brief get_thread_pause_condition
	 * 该函数用于判断线程是否需要暂停
	 * @return 
	 * 返回true则线程睡眠
	 * 默认是返回true，线程启动即睡眠
	 */
	virtual bool get_thread_pause_condition() noexcept override;
private:
	/**
	 * @brief get_next_packet
	 * 从队列里获取下一个包
	 * @return 
	 * 如果失败则返回nullptr
	 */
	core::FramePacket::SharedPacket _get_next_packet() noexcept;
private:
	std::mutex _mutex;
	Queue * _queue;
	AudioEncoderPrivateData * const d_ptr;
};

inline void AudioEncoder::set_input_queue(AudioEncoder::Queue * input_queue) noexcept	{
	std::lock_guard<std::mutex> lk(_mutex);
	auto _q = _queue;
	_queue = input_queue;
	if(_q != nullptr)
		_q->exit_wait_resource();
	if(_queue != nullptr)
		start_thread();
	
}
inline bool AudioEncoder::has_input_queue() noexcept								{		return _queue != nullptr;}
inline const AudioEncoder::Queue * AudioEncoder::get_input_queue() noexcept			{		return _queue;}

} // namespace codec

} // namespace rtplivelib
