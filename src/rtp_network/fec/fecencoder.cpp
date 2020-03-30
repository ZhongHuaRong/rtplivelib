#include "fecencoder.h"
#include "../../core/logger.h"
#include <memory>
#include "codec/wirehair.h"

namespace rtplivelib {

namespace rtp_network {

namespace fec {

class FECEncoderPrivateData{
public:
    /*编码器*/
    Wirehair codec;
    
    FECEncoderPrivateData():
        codec(Wirehair::Encoder){
        
    }
};


///////////////////////////////////////////////

FECEncoder::FECEncoder():
    d_ptr(new FECEncoderPrivateData)
{
    set_symbol_size(1024);
}

FECEncoder::~FECEncoder()
{
    delete d_ptr;
}

void FECEncoder::set_symbol_size(uint32_t value) noexcept
{
    d_ptr->codec.set_packet_size(value);
}

uint32_t FECEncoder::get_symbol_size() noexcept
{
    return d_ptr->codec.get_packet_size();
}

core::Result FECEncoder::encode(core::FramePacket::SharedPacket packet,
                                std::vector<std::vector<int8_t> > &output,
                                FECParam & param) noexcept
{
    if(packet == nullptr)
        return core::Result::Invalid_Parameter;
    
    return core::Result::Success;
}

} //namespace fec

} //namespace rtp_network

} //namespace rtplivelib

