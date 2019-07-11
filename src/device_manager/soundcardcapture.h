#pragma once

#include "abstractcapture.h"

namespace rtplivelib {

namespace device_manager {

struct SoundCardCapturePrivateData;

/**
 * @brief The SoundCardCapture class
 * 声卡采集
 * Windows系统将采用WASAPI实现
 * Linux暂时没有实现
 * 该类在测试阶段暂时不会采用，将在以后完善
 */
class RTPLIVELIBSHARED_EXPORT SoundCardCapture :public AbstractCapture
{
public:
	SoundCardCapture();

    ~SoundCardCapture() override;
	
	virtual std::map<std::string,std::string> get_all_device_info() noexcept override;

    /**
     * @brief set_current_device_name
     * 设置当前设备名字id，要注意的是id,不是设备名字
     * 也就是get_all_device_info返回的value，而不是key
     * 声卡的话还是设置默认设备就可以了
     * @param name
     * 设备id
     * @return
     */
	virtual bool set_current_device_name(std::string name) noexcept override;

protected:
    /**
     * @brief OnStart
     * 开始捕捉音频后的回调
     */
	virtual AbstractCapture::SharedPacket on_start() noexcept override;

    /**
     * @brief OnStart
     * 结束捕捉音频后的回调
     */
	virtual void on_stop() noexcept override;
	
	/**
	 * @brief open_device
	 * 尝试打开设备
	 * @return 
	 */
	bool open_device() noexcept;
private:
	SoundCardCapturePrivateData * const d_ptr;
};

}//namespace device_manager

}//namespace rtplivelib
