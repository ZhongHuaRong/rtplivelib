
#pragma once

#include "fecabstractcodec.h"

namespace rtplivelib {

namespace rtp_network {

namespace fec {

class LDPCCodecPrivateData;

/**
 * @brief The FECEncoder class
 * 用于FEC编码
 */
class RTPLIVELIBSHARED_EXPORT LDPCCodec:public FECAbstractCodec
{
public:
    LDPCCodec(CodecType type,
              uint32_t packet_size = 1024);
    
    LDPCCodec(CodecType type,
              uint32_t packet_size,
              float code_rate);
    
    ~LDPCCodec();
    
    /**
     * @brief set_code_rate
     * 设置原来数据的占比
     * 默认0.8
     * @param code_rate
     */
    void set_code_rate( const float & code_rate) noexcept;
    
    /**
     * @brief get_code_rate
     * 获取已设置的占比
     * @return 
     */
    float get_code_rate() noexcept;
    
    /**
     * @brief encode
     * 编码，当size大于symbol size的时候就会进行分包，然后计算出冗余包的数量后进行编码
     * 当返回true时，可以通过下面三个接口获取数据
     * @param data
     * 源数据
     * @param size
     * 数据大小
     * @return 
     */
    bool encode(void * data,uint32_t size) noexcept;
    
    
    /**
     * @brief decode
     * 解码
     * @return 
     */
    bool decode(uint32_t src_pack_nb,uint32_t repair_pack_nb,uint32_t block_size,
                           void ** src_data,void ** dst_data) noexcept;
    
    /**
     * @brief get_all_pack_nb
     * 获取所有包的数量，等于原包数量+冗余包数量
     * @return 
     */
    uint32_t get_all_pack_nb() noexcept;
    
    /**
     * @brief get_repair_nb
     * 获取冗余包数量
     * @return 
     */
    uint32_t get_repair_nb() noexcept;
    
    /**
     * @brief get_data
     * 获取编码后的数组，该数组所有权归该类所有，不允许在外部释放
     * 该空间的生存周期在两次encode间
     * 注意:encode后，前一次encode的数据将会被释放
     * @return 
     */
    void ** get_data() noexcept;
private:
    LDPCCodecPrivateData * const d_ptr;
};


} //namespace fec

} //namespace rtp_network

} //namespace rtplivelib

