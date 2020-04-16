
#pragma once

#include "../core/abstractqueue.h"
#include "../core/format.h"
#include "../rtp_network/rtpsession.h"
#include "../player/videoplayer.h"
#include "hardwaredevice.h"
#include <memory>

namespace rtplivelib {

namespace codec {

class VideoDecoderPrivateData;

/**
 * @brief The VideoDecoder class
 * 解码类
 * 这里不允许外部设置硬解类型
 * 内部将会选择一项最优方案硬解
 * 当所有方案都不行的时候，将会软解，软解的话，速度性能都跟不上
 */
class RTPLIVELIBSHARED_EXPORT VideoDecoder :
		public core::AbstractQueue<std::pair<rtp_network::RTPSession::PayloadType,std::shared_ptr<core::FramePacket>>>
{
public:
	using Packet = std::pair<rtp_network::RTPSession::PayloadType,std::shared_ptr<core::FramePacket>>;
	using Queue = core::AbstractQueue<Packet>;
public:
	VideoDecoder();
	
	virtual ~VideoDecoder() override;
	
	/**
	 * @brief set_player_object
	 * 临时接口，用于设置播放器
	 */
	void set_player_object(player::VideoPlayer * player) noexcept;
	
	/**
	 * @brief set_hwd_type
	 * 设置硬件加速类型，默认设置为Auto，加速方案启动失败则会设置为None
	 */
	void set_hwd_type(HardwareDevice::HWDType type) noexcept;
	
	/**
	 * @brief get_hwd_type
	 * 获取该解码器的实际使用的硬解加速方案，而不是预设方案
	 * 如果没有使用加速方案，则是返回None
	 * @return 
	 */
	HardwareDevice::HWDType get_hwd_type() noexcept;
protected:
	/**
	 * @brief on_thread_run
	 * 线程运行时需要处理的操作
	 */
	virtual void on_thread_run() noexcept override;
	
	/**
	 * @brief get_thread_pause_condition
	 * 该函数用于判断线程是否需要暂停
	 */
	virtual bool get_thread_pause_condition() noexcept override;
private:
	VideoDecoderPrivateData * const d_ptr;
};

inline bool VideoDecoder::get_thread_pause_condition() noexcept							{		return false;}

}// namespace codec

}// namespace rtplivelib 
