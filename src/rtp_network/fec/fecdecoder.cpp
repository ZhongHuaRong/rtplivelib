#include "fecdecoder.h"
#include "../../core/logger.h"
#include "codec/wirehair.h"

namespace rtplivelib {

namespace rtp_network {

namespace fec {

class FECDecoderPrivateData{
public:
    /*编码器*/
    Wirehair codec;
    
    FECDecoderPrivateData():
        codec(Wirehair::Decoder){
        
    }
   
};

///////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////


FECDecoder::FECDecoder():
    d_ptr(new FECDecoderPrivateData)
{
    
}

FECDecoder::~FECDecoder()
{
    delete d_ptr;
}

core::Result FECDecoder::decode(uint16_t id, const std::vector<int8_t> &input, uint32_t total_size) noexcept
{
    return d_ptr->codec.decode(id,input,total_size);
}

core::Result FECDecoder::data_recover(std::vector<int8_t> &data) noexcept
{
    return d_ptr->codec.data_recover(data);
}


} //namespace fec

} //namespace rtp_network

} //namespace rtplivelib

