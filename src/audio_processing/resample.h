
#pragma once

#include "../core/config.h"
#include "../core/format.h"

namespace rtplivelib{

namespace audio_processing{

class ResamplePrivateData;

/**
 * @brief The Resample class
 * 音频重采样
 * 该类所有操作都是线程安全的
 */
class RTPLIVELIBSHARED_EXPORT Resample
{
public:
    Resample();
    
    ~Resample();
    
    /**
     * @brief set_default_output_format
     * 设置默认输出格式
     */
    void set_default_output_format(const core::Format& format) noexcept;
    
    /**
     * @brief set_default_output_format
     * 重载函数
     */
    void set_default_output_format(const int & sample_rate, const int & channels, const int & bits) noexcept;
    
    /**
     * @brief set_default_intput_format
     * 设置默认的输入格式
     */
    void set_default_input_format(const core::Format& format) noexcept;
    
    /**
     * @brief set_default_input_format
     * 重载函数
     */
    void set_default_input_format(const int & sample_rate, const int & channels, const int & bits) noexcept;
    
    /**
     * @brief scale
     * 重采样
     */
    bool resample(core::FramePacket * dst,core::FramePacket *src) noexcept;
    
    /**
     * @brief scale
     * 重载函数，但是参数不允许出现空指针
     * 智能指针替换指针好麻烦
     */
    bool resample(core::FramePacket::SharedPacket dst,core::FramePacket::SharedPacket src) noexcept;
    
    /**
     * @brief scale
     * 重载函数,不局限于core::FramePacket
     * @param src_data
     * 原数据数组,不允许为nullptr
     * @param src_linesize
     * 原行宽
     * @param dst_data
     * 目标数据数组
     * @param dst_linesize
     * 目标行宽
     * @return 
     */
    bool resample( uint8_t * src_data[],int src_linesize[],
                   uint8_t * dst_data[],int dst_linesize[]) noexcept;
private:
    ResamplePrivateData * const d_ptr;
};

} // namespace audio_processing

} // namespace rtplivelib
