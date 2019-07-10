
#pragma once

#include "../core/abstractqueue.h"
#include "../core/format.h"
#include "../rtp_network/rtpsession.h"
#include "hardwaredevice.h"

class AVCodec;
class AVCodecContext;
class AVFrame;

namespace rtplivelib {

namespace codec {

/**
 * @brief The VideoEncode class
 * 编码器的基类
 * 处理一些编码器相关操作
 * 该类是线程安全类
 */
class RTPLIVELIBSHARED_EXPORT Encoder : 
		public core::AbstractQueue<core::FramePacket::SharedPacket,core::FramePacket::SharedPacket,core::NotDelete>
{
public:
	using Queue = core::AbstractQueue<core::FramePacket::SharedPacket,core::FramePacket::SharedPacket,core::NotDelete>;
public:
	/**
	 * @brief VideoEncode
	 * 不带参数的构造函数，没啥作用，需要调用set_input_queue设置一个输入队列
	 * 简称生产者
	 * @param codec_name
	 * 这个是编码器名字
	 * @param use_hw_accleration
	 * 用于标识是否开启硬件加速,Video一般需要，Audio则不需要
	 */
	Encoder(const char * codec_name,
			bool use_hw_acceleration = true,
			HardwareDevice::HWDType hwa_type = HardwareDevice::Auto);
	
	/**
	 * @brief VideoEncode
	 * 带有输入队列参数的构造函数，将会直接工作
	 * @param input_queue
	 * 输入队列
	 */
	explicit Encoder(Queue * input_queue,
					 const char * codec_name,
					 bool use_hw_acceleration = true,
					 HardwareDevice::HWDType hwa_type = HardwareDevice::Auto);
	
	/**
	 * @brief ~VideoEncoder
	 * 释放资源
	 */
	virtual ~Encoder() override;
	
	/**
	 * @brief set_codec_id
     * 不推荐外部调用该接口
     * 设置编码器
	 * 不允许在编码的时候调用该接口
	 * @param codec_name
	 * 这个是编码器名字
	 * @return 
	 * 是否设置成功
	 */
	bool set_encoder_name(const char * codec_name) noexcept;
	
	/**
	 * @brief get_encoder_name
	 * 获取当前编码器名字
	 * 给外部提供当前是哪种编码格式
	 * @return 
	 */
	std::string get_encoder_name() noexcept;
	
	/**
	 * @brief set_hardware_acceleration
	 * 是否开启硬件加速，
	 * 在Video下，
	 * 默认在调用构造函数的时候传入true开启硬件加速
	 * 如果硬件加速出来的效果不好，而机器性能又不能完全软压的情况下
	 * 请调低分辨率
	 * 在Audio下，不处理
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
	 * @brief encode
	 * 子类需要实现这个接口
	 * 用于实际编码
	 */
	virtual void encode(core::FramePacket * packet) noexcept = 0 ;
	
	/**
	 * @brief creat_encoder
	 * 创建编码器
	 * @param name
	 * 编码器名字
	 * @see set_encoder_param
	 * @see open_encoder
	 * @see close_encoder
	 */
	bool creat_encoder(const char * name) noexcept;
	
	/**
	 * @brief set_encoder_param
	 * 设置编码器参数
	 * 要注意，需要先调用creat_encoder创建编码器，否则不能正常使用该接口
	 * 这个接口也是纯虚的，需要子类实现
	 * @param format
	 * 用于设置的参数
	 * @see creat_encoder
	 */
	virtual void set_encoder_param(const core::Format & format) noexcept = 0;
	
	/**
	 * @brief open_encoder
	 * 开启编码器
	 * 需要提前设置好参数，否则将会打开失败
	 * @see creat_encoder
	 * @see set_encoder_param
	 * @see close_encoder
	 */
	bool open_encoder() noexcept;
	
	/**
	 * @brief close_encoder
	 * 关闭解码器，同时释放内存
	 * @see creat_encoder
	 * @see set_encoder_param
	 * @see open_encoder
	 */
	void close_encoder() noexcept;
	
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
protected:
	//编码器
	AVCodec * encoder{nullptr};
	//编码器上下文
	AVCodecContext * encoder_ctx{nullptr};
	//硬件加速类
	HardwareDevice * hwdevice{nullptr};
	//保存上一次正确编码时的输入格式
	core::Format format;
	//硬件启动标志
	bool use_hw_flag{true};
	//用于用户设置
	HardwareDevice::HWDType hwd_type_user{HardwareDevice::None};
	//目前正在使用的类型,用于判断用户是否修改硬件加速方案
	HardwareDevice::HWDType hwd_type_cur{HardwareDevice::None};
	//有效负载
	rtp_network::RTPSession::PayloadType payload_type{rtp_network::RTPSession::PayloadType::RTP_PT_NONE};
	//编码器同步锁
	std::recursive_mutex encoder_mutex;
private:
	Queue * _queue;
	std::mutex _queue_mutex;
};

inline void Encoder::set_input_queue(Encoder::Queue * input_queue) noexcept	{
	std::lock_guard<std::mutex> lk(_queue_mutex);
	auto _q = _queue;
	_queue = input_queue;
	if(_q != nullptr)
		_q->exit_wait_resource();
	if(_queue != nullptr)
		start_thread();
	
}
inline bool Encoder::has_input_queue() noexcept								{		return _queue != nullptr;}
inline const Encoder::Queue * Encoder::get_input_queue() noexcept			{		return _queue;}

} // namespace codec

} // namespace rtplivelib
