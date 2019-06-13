
#pragma once

#include "../core/config.h"

namespace rtplivelib{

namespace codec{

class HardwareDevicePrivateData;

/**
 * @brief The HardwareDevice class
 */
class RTPLIVELIBSHARED_EXPORT HardwareDevice
{
public:
    enum HWDType{
        None = -1,
        Auto = 0,
        QSV = 1,
        NVIDIA,
        DXVA,
        VideoToolbox,
        AMF
    };
public:
    HardwareDevice();
    
    virtual ~HardwareDevice();
    
    /**
     * @brief get_hwframe_pix_fmt
     * 获取硬件设备支持的像素格式(一般是硬件设备类型)
     * @return 
     */
    virtual int get_hwframe_pix_fmt() noexcept;
    
    /**
     * @brief get_swframe_pix_fmt
     * 获取硬件设备支持的软件帧像素格式(这里一般是NV12,子类实现一般会返回NV12)
     * @return 
     */
    virtual int get_swframe_pix_fmt() noexcept;
    
    /**
     * @brief init_device
     * 初始化设备,将会打开编解码器
     * @param codec_ctx
     * 编解码器上下文,需要提前初始化
     * @return 
     * 成功则返回真
     */
    virtual bool init_device(void * codec_ctx,void * codec,HWDType type) noexcept;
    
    /**
     * @brief close_device
     * 关闭设备，将会关闭编解码器，需要提前终止发送帧
     * 清空缓存帧
     * @param codec_ctx
     * 编解码器上下文
     * @return 
     */
    virtual bool close_device(void * codec_ctx) noexcept;
    
    /**
     * @brief get_hwd_type
     * 获取目前设置的类型
     * @return 
     */
    HWDType get_hwd_type() noexcept;
    
    /**
     * @brief get_init_result
     * 获取硬件设备初始化结果
     * @return 
     */
    bool get_init_result() noexcept;
private:
    HWDType _type;
    HardwareDevicePrivateData * const d_ptr;
    bool _init_result;
};

inline HardwareDevice::HWDType HardwareDevice::get_hwd_type() noexcept {
    return _type;
}

inline bool HardwareDevice::get_init_result() noexcept {
    return _init_result;
}

} // namespace codec

} // namespace rtplivelib
