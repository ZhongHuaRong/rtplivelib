

#pragma once

#include "../../core/config.h"
#include "../../core/format.h"
#include "../../core/error.h"
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
     * @param id
     * 包的编号
     * @param input
     * 编码数据
     * @param total_size
     * 编码前的数据总大小
     * @return 
     * Success : 解码成功，调用data_recover获取复原的数据
     * Need_More : 需要输入更多的包解码
     * Decode_Failed : 解码失败,解码失败后将会释放相关上下文，准备开始下次的解码
     */
    virtual core::Result decode(uint16_t id,
                                const std::vector<int8_t> &input,
                                uint32_t total_size) noexcept;
    
    /**
     * @brief decode
     * 重载函数
     * @return 
     */
    virtual core::Result decode(RTPPacket * packet) noexcept;
    
    /**
     * @brief data_recover
     * 获取修复后的数据
     * @param data
     * 将会输出到该vector
     * @return 
     * Success : 获取数据成功,成功后将会释放相关上下文，准备开始下次的解码
     * Need_More : 需要输入更多的包解码
     */
    virtual core::Result data_recover(std::vector<int8_t> & data) noexcept;
private:
    FECDecoderPrivateData * const d_ptr;
};


} //namespace fec

} //namespace rtp_network

} //namespace rtplivelib

