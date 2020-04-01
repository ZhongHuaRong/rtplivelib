
#pragma once

#include "../core/config.h"
#include "../core/format.h"
#include "../core/error.h"

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
     * @brief resample
     * 重采样
     */
    core::Result resample(core::FramePacket * dst,core::FramePacket *src) noexcept;
    
    /**
     * @brief resample
     * 重载函数，但是参数不允许出现空指针
     */
    core::Result resample(core::FramePacket::SharedPacket &dst,core::FramePacket::SharedPacket &src) noexcept;
    
    /**
     * @brief resample
     * 重载函数,不局限于core::FramePacket
     * 该接口不会检查格式是否正确,如果需要检查格式建议使用其他重载函数
     * @param src_data
     * 原数据数组,不允许为nullptr
     * @param src_nb_samples
     * 输入的样本数
     * @param dst_data
     * 目标数据数组,采样成功后，原数组的数据将会擦除
     * 这个参数最好是nullptr，即*dst_data == nullptr
     * @param dst_nb_samples
     * 将要输出的样本数，这个参数将会在该接口内赋值
     * @param buffer_size
     * 返回输出音频所占用的空间大小
     * @return 
     */
    core::Result resample( uint8_t *** src_data,const int &src_nb_samples,
                           uint8_t *** dst_data,int & dst_nb_samples,
                           int & buffer_size) noexcept;
private:
    ResamplePrivateData * const d_ptr;
};

} // namespace audio_processing

} // namespace rtplivelib
