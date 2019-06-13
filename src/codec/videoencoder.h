
#pragma once

#include "../core/abstractqueue.h"
#include "../core/format.h"
#include "hardwaredevice.h"

namespace rtplivelib {

namespace codec {

class VideoEncoderPrivateData;

/**
 * @brief The VideoEncode class
 * 编码默认采用的是HEVC
 * 当HEVC无法使用的时候将会启用H264
 * 硬件加速事默认启动的，将会寻找最适合的显卡作为加速方案
 * (Linux和Windows)
 * N卡采用NVENC
 * Intel核显采用qsv
 */
class RTPLIVELIBSHARED_EXPORT VideoEncoder : 
		public core::AbstractQueue<core::FramePacket::SharedPacket,core::FramePacket::SharedPacket,core::NotDelete>
{
public:
	using Queue = core::AbstractQueue<core::FramePacket::SharedPacket,core::FramePacket::SharedPacket,core::NotDelete>;
public:
	/**
	 * @brief VideoEncode
	 * 不带参数的构造函数，没啥作用，需要调用set_input_queue设置一个输入队列
	 * 简称生产者
	 * 不提供codec id,只是暴露编码器id修改的接口
	 * @param codec_id
	 * 这个是编码器id,173是HEVC
	 */
	VideoEncoder(int codec_id = 173,
				 bool use_hw_acceleration = true,
				 HardwareDevice::HWDType hwa_type = HardwareDevice::Auto);
	
	/**
	 * @brief VideoEncode
	 * 带有输入队列参数的构造函数，将会直接工作
	 * @param input_queue
	 * 输入队列
	 * @param codec_id
	 * 这个是编码器id,173是HEVC
	 */
	explicit VideoEncoder(Queue * input_queue,
						  int codec_id = 173,
						  bool use_hw_acceleration = true,
						  HardwareDevice::HWDType hwa_type = HardwareDevice::Auto);
	
	/**
	 * @brief ~VideoEncoder
	 * 释放资源
	 */
	virtual ~VideoEncoder() override;
	
	/**
	 * @brief set_codec_id
     * 不推荐外部调用该接口
     * 设置编码器
	 * 不允许在编码的时候调用该接口
	 * 不过不提供id,内部调动的接口
	 * @param codec_id
	 * 这个是编码器id,173是HEVC
	 * @return 
	 * 是否设置成功
	 */
	bool set_encoder_id(int codec_id) noexcept;
	
	/**
	 * @brief get_codec_id
	 * 获取当前编码器id
	 * 给外部提供当前是哪种编码格式
	 * @return 
	 */
	int get_encoder_id() noexcept;
	
	/**
	 * @brief set_hardware_acceleration
	 * 是否开启硬件加速，
	 * 默认在调用构造函数的时候传入true开启硬件加速
	 * 如果硬件加速出来的效果不好，而机器性能又不能完全软压的情况下
	 * 请调低分辨率
	 * @param hwa_type
	 * 硬件设备类型，默认是自动选择
	 * 也可以手动选择，当选择的硬件设备无法打开的时候将会使用纯CPU进行编码
	 */
	void set_hardware_acceleration(bool flag,HardwareDevice::HWDType hwa_type = HardwareDevice::Auto) noexcept;
	
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
	VideoEncoderPrivateData * const d_ptr;
};

inline void VideoEncoder::set_input_queue(VideoEncoder::Queue * input_queue) noexcept	{
	std::lock_guard<std::mutex> lk(_mutex);
	auto _q = _queue;
	_queue = input_queue;
	if(_q != nullptr)
		_q->exit_wait_resource();
	if(_queue != nullptr)
		start_thread();
	
}
inline bool VideoEncoder::has_input_queue() noexcept								{		return _queue != nullptr;}
inline const VideoEncoder::Queue * VideoEncoder::get_input_queue() noexcept			{		return _queue;}

} // namespace codec

} // namespace rtplivelib
