#include "wirehair.h"
#include "wirehair/wirehair.h"
#include "../../../core/logger.h"
#include <memory>

namespace rtplivelib {

namespace rtp_network {

namespace fec {

struct WirehairFree{
    void operator() (WirehairCodec_t * p){
        if(p!=nullptr)
            wirehair_free(p);
    }
};

struct WirehairPrivateData{
    ///编解码器上下文
    std::shared_ptr<WirehairCodec_t>    codec;
    void                                *data{nullptr};
    uint32_t                             data_size;
    uint32_t                             recover_data_size;
};

Wirehair::Wirehair(FECAbstractCodec::CodecType type, uint32_t packet_size):
    FECAbstractCodec(FountainCodes,type),
    d_ptr(new WirehairPrivateData)
{
    if(type == NotSetType || packet_size == 0)
        return;
    
    set_packet_size(packet_size);
}

Wirehair::~Wirehair()
{
    delete d_ptr;
}

void Wirehair::set_encode_data(void *data, uint32_t size) noexcept
{
    d_ptr->data = data;
    d_ptr->data_size = size;
    d_ptr->codec.reset();
}

core::Result Wirehair::encode(uint16_t id, 
                              std::vector<int8_t> &output,
                              uint32_t &output_size) noexcept
{
    if(get_codec_type() != CodecType::Encoder){
        return core::Result::FEC_Codec_Not_Encoder;
    }
    if(d_ptr->codec == nullptr){
        auto p = wirehair_encoder_create(nullptr,d_ptr->data, d_ptr->data_size, get_packet_size());
        if(p == nullptr)
            return core::Result::FEC_Codec_Open_Failed;
        d_ptr->codec.reset(p,WirehairFree());
    }
    
    output_size = 0;
    if(output.size() < get_packet_size()){
        output.resize(get_packet_size());
    } 
    WirehairResult encodeResult = wirehair_encode(
        d_ptr->codec.get(), 
        id, 
        output.data(),
        get_packet_size(),
        &output_size);

    if (encodeResult != Wirehair_Success){
        core::Logger::Print(wirehair_result_string(encodeResult),
							__PRETTY_FUNCTION__,
                            LogLevel::WARNING_LEVEL);
        return core::Result::FEC_Encode_Failed;
    }
    
    return core::Result::Success;
}

core::Result Wirehair::encode(void *data,
                              uint32_t size, 
                              uint32_t count, 
                              std::vector<std::vector<int8_t> > &output) noexcept
{
    if(get_codec_type() != CodecType::Encoder){
        return core::Result::FEC_Codec_Not_Encoder;
    }
    uint32_t packet_size = get_packet_size();
    WirehairCodec encoder = wirehair_encoder_create(nullptr, data, size, packet_size);
    if (!encoder){
        return core::Result::FEC_Codec_Open_Failed;
    } else {
        d_ptr->codec.reset(encoder,WirehairFree());
    }
    
    
    std::vector<int8_t> tmp(packet_size);
    uint32_t output_size{0};
    WirehairResult encodeResult;
    
    output.resize(count);
    for(uint32_t id = 0;id < count;++id){
        encodeResult = wirehair_encode(
            d_ptr->codec.get(), 
            id, 
            tmp.data(),
            packet_size,
            &output_size);
    
        if (encodeResult != Wirehair_Success){
            core::Logger::Print(wirehair_result_string(encodeResult),
								__PRETTY_FUNCTION__,
                                LogLevel::WARNING_LEVEL);
            return core::Result::FEC_Encode_Failed;
        }
        
        std::vector<int8_t> ret(output_size);
        memcpy(ret.data(),tmp.data(),output_size);
        output[id] = ret;
    }
    return core::Result::Success;
}

core::Result Wirehair::encode(void * data,
                              uint32_t size,
                              float rate,
                              std::vector<std::vector<int8_t>> & output) noexcept
{
    
    if(rate < 0.001f || rate >= 1.0f){
        return core::Result::Invalid_Parameter;
    }
    
    uint32_t && packet_size = get_packet_size();
    auto src_nb = static_cast<float>(size) / packet_size;
    auto total_nb = static_cast<uint32_t>(src_nb / rate) + 5;

    return encode(data,size,total_nb,output);
}

core::Result Wirehair::decode(uint16_t id, void *data, uint32_t size, uint32_t total_size) noexcept
{
    if(get_codec_type() != CodecType::Decoder){
        return core::Result::FEC_Codec_Not_Decoder;
    }
    
    if(d_ptr->codec == nullptr){
        WirehairCodec decoder = wirehair_decoder_create(nullptr, total_size, get_packet_size());
        if (!decoder){
            return core::Result::FEC_Codec_Open_Failed;
        } else {
            d_ptr->codec.reset(decoder,WirehairFree());
            d_ptr->recover_data_size = total_size;
        }
    }
    
    WirehairResult decodeResult = wirehair_decode(
        d_ptr->codec.get(),
        id,
        data, 
        size);

    if (decodeResult == Wirehair_NeedMore) {
        return core::Result::FEC_Decode_Need_More;
    } else if (decodeResult == Wirehair_Success){
        return core::Result::Success;
    } else {
        d_ptr->codec.reset();
        core::Logger::Print(wirehair_result_string(decodeResult),
							__PRETTY_FUNCTION__,
                            LogLevel::WARNING_LEVEL);
        return core::Result::FEC_Decode_Failed;
    }
}

core::Result Wirehair::data_recover(std::vector<int8_t> &data) noexcept
{
    data.resize(d_ptr->recover_data_size);
    return data_recover(data.data());
}

core::Result Wirehair::data_recover(void *data) noexcept
{
    if(get_codec_type() != CodecType::Decoder){
        return core::Result::FEC_Codec_Not_Decoder;
    }
    
    if(d_ptr->codec == nullptr){
        return core::Result::FEC_Codec_Open_Failed;
    }
    
    WirehairResult decodeResult = wirehair_recover(
        d_ptr->codec.get(),
        data,
        d_ptr->recover_data_size);
    
    d_ptr->codec.reset();
    if (decodeResult != Wirehair_Success){
        core::Logger::Print(wirehair_result_string(decodeResult),
							__PRETTY_FUNCTION__,
                            LogLevel::WARNING_LEVEL);
        return core::Result::FEC_Decode_Failed;
    }
    
    return core::Result::Success;
}

bool Wirehair::InitCodec() noexcept
{
    const WirehairResult initResult = wirehair_init();

    if (initResult != Wirehair_Success)
    {
        core::Logger::Print(wirehair_result_string(initResult),
							__PRETTY_FUNCTION__,
                            LogLevel::WARNING_LEVEL);
        return false;
    } 
    return true;
}

} //namespace fec

} //namespace rtp_network

} //namespace rtplivelib

