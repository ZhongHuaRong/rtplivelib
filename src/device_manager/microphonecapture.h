
#pragma once

#include "abstractcapture.h"

namespace rtplivelib {

namespace device_manager {

class MicrophoneCapturePrivateData;

/**
 * @brief The MicrophoneCapture class
 * 该类负责麦克风音频捕捉
 * 在类初始化后将会尝试打开设备
 * 如果打开设备失败则说明该系统不支持
 * 备注:
 * Windows系统
 * 使用WASAPI
 * Linux系统
 * 
 * 如果需要用该类自己处理数据，则可以考虑继承该类，然后重写on_frame_data接口来前处理数据
 * 然后返回false，也可以用wait_resource_push()和get_next()来获取队列的数据
 * 也可以通过实例化DeviceCapture类来捕捉，该类已经封装好其他四个类了，然后关闭其他三个的捕捉
 */
class RTPLIVELIBSHARED_EXPORT MicrophoneCapture :public AbstractCapture
{
public:
	MicrophoneCapture();

    ~MicrophoneCapture() override;
	
	/**
	 * @brief get_all_device_info
	 * 获取所有设备的信息，其实也就是名字而已
	 * 这里说明一下，key是面向程序的，value是面向用户的
	 * key存的是设备id(外部调用可以忽略该字段)，value存的是设备名字(对用户友好)
	 * @return 
	 * 返回一个map
	 */
	virtual std::map<std::string,std::string> get_all_device_info() noexcept(false) override;
	
	/**
	 * @brief set_current_device
	 * 通过设备id更换当前设备
	 * 获取设备信息后，使用key来获取id，value是设备名字
	 * 如果失败则返回false
	 * @see get_all_device_info
	 */
	virtual bool set_current_device(std::string device_id) noexcept override;
	
	/**
	 * @brief set_default_device
	 * 将当前设备更换成默认设备,简易接口
	 * @return 
	 */
	virtual bool set_default_device() noexcept override;
protected:
    /**
     * @brief on_start
     * 开始捕捉音频后的回调
     */
	virtual AbstractCapture::SharedPacket on_start() noexcept override;

    /**
     * @brief on_stop
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
	MicrophoneCapturePrivateData * const d_ptr;
};

}//namespace device_manager

}//namespace rtplivelib
