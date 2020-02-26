

#pragma once

#include "../../core/config.h"
#include "../../core/format.h"

namespace rtplivelib {

namespace rtp_network {

class RTPPacket;

namespace fec {

class FECDecoderPrivateData;

/**
 * @brief The FECDecoder class
 * 该类负责FEC解码
 * 简化了该类的使用，只提供一个简易接口
 */
class RTPLIVELIBSHARED_EXPORT FECDecoder
{
public:
    FECDecoder();
    
    ~FECDecoder();
    
    /**
     * @brief decode
     * 解码，传入RTPPacket包
     * 如果解码失败则返回nullptr
     * @return 
     */
    core::FramePacket * decode(RTPPacket * packet) noexcept;
private:
    FECDecoderPrivateData * const d_ptr;
};


} //namespace fec

} //namespace rtp_network

} //namespace rtplivelib

