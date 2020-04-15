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
	
	/**
	 * @brief get_all_device_info
	 * 获取所有设备的信息，其实也就是名字而已
	 * 这里说明一下，key是面向程序的，value是面向用户的
	 * key存的是设备id(外部调用可以忽略该字段)，value存的是设备名字(对用户友好)
	 * @return 
	 * 返回一个map
	 */
	virtual std::map<std::string,std::string> get_all_device_info()  noexcept(false) override;
	
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
