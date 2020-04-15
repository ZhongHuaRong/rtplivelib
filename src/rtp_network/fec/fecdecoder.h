

#pragma once

#include "../../core/config.h"
#include "../../core/format.h"
#include "../../core/error.h"
#include "../rtppacket.h"

namespace rtplivelib {

namespace rtp_network {

namespace fec {

class FECDecoderPrivateData;

/**
 * @brief The FECDecoder class
 * 该类负责FEC解码
 */
class RTPLIVELIBSHARED_EXPORT FECDecoder
{
public:
	FECDecoder();
	
	virtual ~FECDecoder();
	
	/**
	 * @brief decode
	 * 解码
	 * @return 
	 */
	virtual core::Result decode(RTPPacket::SharedRTPPacket packet) noexcept;
	
	/**
	 * @brief get_packet
	 * 当解码成功后，调用该接口获取数据
	 * @return 
	 */
	virtual core::FramePacket::SharedPacket get_packet() noexcept;
private:
	FECDecoderPrivateData * const d_ptr;
};


} //namespace fec

} //namespace rtp_network

} //namespace rtplivelib

