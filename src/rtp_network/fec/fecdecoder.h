

#pragma once

#include "../../core/config.h"
#include "../../core/format.h"
#include "../../core/error.h"
#include "../rtppacket.h"
#include <vector>

namespace rtplivelib {

namespace rtp_network {

class RTPPacket;

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
    
    ~FECDecoder();
    
    /**
     * @brief decode
     * 解码
     * @return 
     */
    virtual core::Result decode(RTPPacket::SharedRTPPacket packet) noexcept;
private:
    FECDecoderPrivateData * const d_ptr;
};


} //namespace fec

} //namespace rtp_network

} //namespace rtplivelib

