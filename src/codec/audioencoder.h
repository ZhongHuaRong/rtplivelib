
#pragma once

#include "encoder.h"
#include "../audio_processing/resample.h"

namespace rtplivelib {

namespace codec {

/**
 * @brief The VideoEncode class
 * 编码默认采用的是AAC,也只有AAC
 */
class RTPLIVELIBSHARED_EXPORT AudioEncoder : public Encoder
{
public:
	/**
	 * @brief AudioEncoder
	 * 不带参数的构造函数，没啥作用，需要调用set_input_queue设置一个输入队列
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
     * @brief set_hardware_acceleration
     * 音频编码不需要硬件加速
     */
    void set_hardware_acceleration(bool flag,HardwareDevice::HWDType hwa_type = HardwareDevice::Auto) noexcept = delete;
protected:
	/**
	 * @brief encode
	 * 编码
	 */
	virtual void encode(core::FramePacket * packet) noexcept override;
	
	/**
	 * @brief set_encoder_param
	 * 设置编码器参数
	 * 要注意，需要先调用creat_encoder创建编码器，否则不能正常使用该接口
	 * @param format
	 * 用于设置的参数
	 * @see creat_encoder
	 */
	virtual void set_encoder_param(const core::Format & format) noexcept override;
	
	/**
	 * @brief receive_packet
	 * 接受编码成功后的包
	 */
	void receive_packet() noexcept;
	
private:	
	/**
	 * @brief _alloc_encode_frame
	 * 为编码帧分配空间
	 * @return 
	 */
	bool _alloc_encode_frame() noexcept;
	
	/**
	 * @brief open_ctx
	 * 打开编码器,对父类的open_encoder接口进行了封装
	 */
	bool _open_ctx(const core::FramePacket * packet) noexcept;
	
	/**
	 * @brief _init_resample
	 * 初始化采样器，如果分配失败，则该次编码跳过
	 * @return 
	 */
	bool _init_resample() noexcept;
private:
	//用于保存上一次输入时的音频格式，通过前后对比来重新设置编码器参数
	//现在改为格式不一致则进行重采样(本意是不想通过重采样来处理的,但是好像只有S16是可以成功打开编码器)
	core::Format ifmt;
	//用于重采样的默认格式
	const core::Format default_resample_format{0,0,0,44100,2,16};
	//用于判断是否给frame设置参数
	bool reassignment{false};
	//实际用去编码的数据结构
	AVFrame * encode_frame{nullptr};
	//当输入格式不符合编码格式时则需要重采样
	audio_processing::Resample * resample{nullptr};
};

} // namespace codec

} // namespace rtplivelib
