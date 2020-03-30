#pragma once

#include "../../../core/config.h"
#include "../../../core/error.h"
#include "fecabstractcodec.h"

namespace rtplivelib {

namespace rtp_network {

namespace fec {

struct WirehairPrivateData;

/**
 * @brief The Wirehair class
 * 该编码是一种喷泉码
 * 效率参考https://github.com/catid/wirehair的文档
 * 
 */
class RTPLIVELIBSHARED_EXPORT Wirehair:public FECAbstractCodec
{
public:
    /**
     * @brief Wirehair
     * 初始化的时候需要指明是哪种类型，解码器 or 编码器
     * 同时需要设置块大小
     */
    Wirehair(CodecType type,
            uint32_t packet_size = 1024);
    
    virtual ~Wirehair();
    
    
    /**
     * @brief encode
     * 用于编码原符号
     * @param data
     * 源数据
     * @param size
     * 数据大小
     * @param count
     * 生成的包的数量
     * @param output
     * 输出的数据
     */
    virtual core::Result encode(void * data,
                                uint32_t size,
                                uint32_t count,
                                std::vector<std::vector<int8_t>> & output) noexcept;
    
    /**
     * @brief encode
     * 重载函数
     * 用于编码原符号
     * 由于喷泉码无码率的特点，rate参数表示将要编码的包数= size / packet_size / rate
     */
    virtual core::Result encode(void * data,
                                uint32_t size,
                                float rate,
                                std::vector<std::vector<int8_t>> & output) noexcept;
    
    /**
     * @brief set_data
     * 设置将要编码的数据，与core::Result encode(uint16_t id,std::vector<int8_t>&output)配合使用
     * 此函数调用后，不拥有data内存块的所有权，需要在外部释放该内存块
     * 内存块释放后使用encode则会发生访问错误
     */
    virtual void set_encode_data(void * data,uint32_t size) noexcept;
    
    /**
     * @brief encode
     * 编码，重载函数
     * 一次编码一个包
     * 需要提前使用set_data来设置数据源
     * output数组里，output_size才是有效数据长度，而不是output.size(),output_size <= output.size()
     * id为当前编码输出的包的编号
     * output保存编码后的数据
     * output_size则是有效数据的长度
     */
    virtual core::Result encode(uint16_t id,
                                std::vector<int8_t> & output,
                                uint32_t &output_size) noexcept;
    
    
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
     * @brief data_recover
     * 获取修复后的数据
     * @param data
     * 将会输出到该vector
     * @return 
     * Success : 获取数据成功,成功后将会释放相关上下文，准备开始下次的解码
     * Need_More : 需要输入更多的包解码
     */
    virtual core::Result data_recover(std::vector<int8_t> & data) noexcept;
    
    /**
     * @brief InitCodec
     * 调用该类的编解码功能需要先调用该接口初始化，只需要初始化一次即可
     * @return 
     */
    static bool InitCodec() noexcept;
private:
    WirehairPrivateData * const d_ptr;
};

} //namespace fec

} //namespace rtp_network

} //namespace rtplivelib

