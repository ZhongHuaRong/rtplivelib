
#pragma once

#include "../../core/config.h"
#include "../../core/error.h"
#include <vector>
#include "../../core/format.h"

namespace rtplivelib {

namespace rtp_network {

namespace fec {

class FECEncoderPrivateData;

/**
 * @brief The FECParam struct
 * FEC解码所需的参数，编码完成后获取该参数用于解码
 */
struct FECParam{
    /*冗余包数量，用于LDPC解码*/
    uint32_t repair_nb{0};
    /*原包总数据大小，用于喷泉吗解码*/
    int32_t size{0};
    /*一个包的大小*/
    int32_t symbol_size{0};
    /*用于表示是否使用了FEC编码
     * 0:没使用
     * 非0:使用了
     */
    int32_t flag{0};
    
    inline int32_t get_fill_size() noexcept{
       return size -  size % symbol_size;
    }
    
    inline int32_t get_src_nb() noexcept{
        int32_t n = size / symbol_size;
        return n * symbol_size == size?n:n + 1;
    }
};

/**
 * @brief The FECEncoder class
 * 用于FEC编码
 */
class RTPLIVELIBSHARED_EXPORT FECEncoder
{
public:
    FECEncoder();
    
    ~FECEncoder();

    /**
     * @brief set_symbol_size
     * 设置块大小(每个包的大小)
     * @param value
     */
    void set_symbol_size(uint32_t value) noexcept;
    
    /**
     * @brief get_symbol_size
     * 获取块大小
     * @return 
     */
    uint32_t get_symbol_size() noexcept;
    
    /**
     * @brief encode
     * 编码
     * @param packet
     * 源数据
     * @param output
     * 输出,原来的数据将会被擦除
     * @return 
     * 编码失败也会设置param
     */
    virtual core::Result encode(core::FramePacket::SharedPacket packet,
                                std::vector<std::vector<int8_t>> & output,
                                FECParam & param) noexcept;
private:
    FECEncoderPrivateData * const d_ptr;
};


} //namespace fec

} //namespace rtp_network

} //namespace rtplivelib

