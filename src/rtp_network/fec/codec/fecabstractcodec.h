
#pragma once

#include "../../../core/config.h"
#include "../../../core/error.h"

namespace rtplivelib {

namespace rtp_network {

namespace fec {

/**
 * @brief The FECAbstractCodec class
 * FEC编解码器的基类
 * 实现了编解码器的一些通用接口
 */
class RTPLIVELIBSHARED_EXPORT FECAbstractCodec
{
public:
    enum CodesType{
        NotSetCodes,
        LDPCCodes,
        FountainCodes
    };
    
    enum CodecType{
        NotSetType,
        Encoder,
        Decoder
    };

public:
    /**
     * @brief FECAbstractCodec
     * 编解码器的初始化函数,需要初始化编码类型和编解码器类型
     * @param codesT
     * 编码类型
     * @param codecT
     * 编解码器类型
     */
    FECAbstractCodec(CodesType codesT = NotSetCodes,
                     CodecType codecT = NotSetType);
    
    virtual ~FECAbstractCodec();
    
    /**
     * @brief get_codes_type
     * 获取编码类型
     */
    CodesType get_codes_type() noexcept;
    
    /**
     * @brief get_codec_type
     * 获取编解码器类型（编码器，解码器）
     */
    CodecType get_codec_type() noexcept;
    
    /**
     * @brief set_symbol_size
     * 设置块大小(每个包的大小)
     * @param value
     */
    void set_packet_size(uint32_t value) noexcept;
    
    /**
     * @brief get_symbol_size
     * 获取块大小
     * @return 
     */
    uint32_t get_packet_size() noexcept;
protected:
    /**
     * @brief set_codes_type
     * 设置编码类型,子类需要设置好类型
     */
    void set_codes_type(CodesType type) noexcept;
    
    /**
     * @brief set_codec_type
     * 设置编解码器类型,子类需要设置好类型
     */
    void set_codec_type(CodecType type) noexcept;
private:
    /** 单个包的大小，默认1024 **/
    uint32_t packet_size{1024};
    CodesType codes_type{CodesType::NotSetCodes};
    CodecType codec_type{CodecType::NotSetType}; 
};


inline FECAbstractCodec::CodesType FECAbstractCodec::get_codes_type() noexcept          {   return codes_type;}
inline FECAbstractCodec::CodecType FECAbstractCodec::get_codec_type() noexcept          {   return codec_type;}
inline void FECAbstractCodec::set_packet_size(uint32_t value) noexcept                  {   packet_size = value;}
inline uint32_t FECAbstractCodec::get_packet_size() noexcept                            {   return packet_size;}

inline void FECAbstractCodec::set_codes_type(FECAbstractCodec::CodesType type) noexcept {    codes_type = type;}
inline void FECAbstractCodec::set_codec_type(FECAbstractCodec::CodecType type) noexcept {    codec_type = type;}


} //namespace fec

} //namespace rtp_network

} //namespace rtplivelib

