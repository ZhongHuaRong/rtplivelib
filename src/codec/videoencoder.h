
#pragma once

#include "encoder.h"
#include "../image_processing/scale.h"

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
class RTPLIVELIBSHARED_EXPORT VideoEncoder : public Encoder
{
public:
	/**
	 * @brief VideoEncode
	 * 不带参数的构造函数，没啥作用，需要调用set_input_queue设置一个输入队列
	 */
	VideoEncoder(bool use_hw_acceleration = true,
				 HardwareDevice::HWDType hwa_type = HardwareDevice::Auto,
				 EncoderType enc_type = EncoderType::Auto);
	
	/**
	 * @brief VideoEncode
	 * 带有输入队列参数的构造函数，将会直接工作
	 */
	explicit VideoEncoder(Queue * input_queue,
						  bool use_hw_acceleration = true,
						  HardwareDevice::HWDType hwa_type = HardwareDevice::Auto,
						  EncoderType enc_type = EncoderType::Auto);
	
	/**
	 * @brief ~VideoEncoder
	 * 释放资源
	 */
	virtual ~VideoEncoder() override;
protected:
	/**
	 * @brief encode
	 * 编码
	 */
	virtual void encode(core::FramePacket::SharedPacket packet) noexcept override;
	
	/**
	 * @brief set_encoder_param
	 * 设置编码器参数
	 * 要注意，需要先调用creat_encoder创建编码器，否则不能正常使用该接口
	 * @param format
	 * 用于设置的参数
	 * @see creat_encoder
	 */
	virtual void set_encoder_param(const core::Format & format) noexcept;
	
	/**
	 * @brief receive_packet
	 * 接受编码成功后的包
	 */
	void receive_packet() noexcept;
private:
	/**
	 * @brief _init_hwdevice
	 * 初始化硬件设备并启动编码器
	 * 将会优先选择HEVC，如果不支持则换成264
	 * @param hwdtype
	 * 硬件设备类型
	 * @param format
	 * 图像格式
	 * @return 
	 */
	bool _init_hwdevice(HardwareDevice::HWDType hwdtype,const core::Format& format) noexcept;
	
	/**
	 * @brief _select_hwdevice
	 * 根据当前情况选择最优的硬件加速方案
	 * 目前只设置qsv
	 * @param packet
	 * 将要编码的包信息
	 * @return 
	 */
	bool _select_hwdevice(const core::FramePacket::SharedPacket packet) noexcept;
	
	/**
	 * @brief _set_sw_encoder_ctx
	 * 设置编码器上下文并打开编码器
	 * 这个接口将会启动纯CPU进行编码
	 * 在输入图像的时候，要严格按照格式设置
	 * @param dst_format
	 * 目标格式
	 * @param fps
	 * 每秒帧数 
	 */
	void _set_sw_encoder_ctx(const core::FramePacket::SharedPacket packet) noexcept;
	
	/**
	 * @brief _close_ctx
	 * 关闭上下文,同时也会释放帧
	 */
	void _close_ctx() noexcept;
	
	/**
	 * @brief _alloc_frame
	 * 分配并初始化AVFrame
	 * 会根据提前设定好的格式进行初始化，不需要额外输入格式,简化操作
	 */
	AVFrame * _alloc_frame() noexcept;
	
	/**
	 * @brief _alloc_hw_frame
	 * 因为硬件帧和软件帧不一样，分开实现
	 * 这个函数必须在硬件编码器上下文初始化后调用
	 * @param packet
	 * 给硬件帧分配空间
	 * @return 
	 */
	AVFrame * _alloc_hw_frame() noexcept;
	
	/**
	 * @brief _free_frame
	 * 释放软件帧和硬件帧
	 */
	void _free_frame() noexcept;
	
	/**
	 * @brief _set_frame_data
	 * 通过packet设置AVFAVFrame的数据
	 * @return 
	 * 如果设置失败则返回false
	 */
	bool _set_frame_data(AVFrame * frame,core::FramePacket::SharedPacket packet) noexcept;
private:
	//硬件加速类
	HardwareDevice								* hwdevice{nullptr};
	//保存上一次正确编码时的输入格式
	core::Format								format;
	//用于格式转换
	std::shared_ptr<image_processing::Scale>	scale_ctx;
	//用来编码的数据结构，只在设置format的时候分配一次
	//随着格式的改变和上下文一起重新分配
	AVFrame										* encode_sw_frame{nullptr};
	AVFrame										* encode_hw_frame{nullptr};
};

} // namespace codec

} // namespace rtplivelib
