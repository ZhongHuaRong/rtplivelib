
#pragma once

#include "abstractcapture.h"

namespace rtplivelib {

namespace device_manager {

class CameraCapturePrivateData;

/**
 * @brief The CameraCapture class
 * 该类负责摄像头的捕捉
 * 其实该类很多操作都和桌面捕捉一样，所以等以后有空会统一一下
 * 备注:
 * Windows系统
 * 采集设备用的是dshow，说实话，辣鸡东西来的
 * 摄像头默认采集的格式是yuyv422
 * Linux系统
 * 采用v4l2采集摄像头，老实说，这个是所有采集里面实现的最好的
 * 所有接口都已实现，可以枚举摄像头
 * 采集的格式和Windows一样，都是yuyv422
 * 
 * 设备获取接口Windows的还没有实现
 * 
 * 如果需要用该类自己处理数据，则可以考虑继承该类，然后重写on_frame_data接口来前处理数据
 * 然后返回false，也可以用wait_resource_push()和get_next()来获取队列的数据
 * 也可以通过实例化DeviceCapture类来捕捉，该类已经封装好其他四个类了，然后关闭其他三个的捕捉
 */
class RTPLIVELIBSHARED_EXPORT CameraCapture : public AbstractCapture
{
public:
	explicit CameraCapture();
	
	~CameraCapture() override;

    /**
     * @brief set_fps
     * 设置帧数
     * @param value
     * 帧数
     */
	void set_fps(int value);
	
	/**
	 * @brief get_fps
	 * 获取预设的帧数
	 */
	int get_fps() noexcept;
	
	/**
	 * @brief get_all_device_info
	 * 获取所有设备的信息，其实也就是名字而已
	 * 这里说明一下，key是面向用户的，value是面向程序的
	 * key存的是设备名字，value存的是程序需要调用的字符串
	 * @return 
	 * 返回一个map
	 * @exception
	 * func_not_implemented_error异常
	 */
	virtual std::map<std::string,std::string> get_all_device_info() noexcept(false) override;
	
	/**
	 * @brief set_current_device_name
	 * 根据名字设置当前设备，成功则返回true，失败则返回false 
	 */
	virtual bool set_current_device_name(std::string name) noexcept override;

protected:
    /**
     * @brief on_start
     * 开始捕捉画面后的回调
     */
	virtual SharedPacket on_start() noexcept override;

    /**
     * @brief on_stop
     * 结束捕捉画面后的回调
     */
	virtual void on_stop() noexcept override;
	
	/**
	 * @brief open_device
	 * 尝试打开设备
	 * @return 
	 */
	bool open_device() noexcept;
private:
	int _fps;
	CameraCapturePrivateData * const d_ptr;
};

inline int CameraCapture::get_fps() noexcept												{		return _fps;}

}//namespace device_manager

}//namespace rtplivelib
