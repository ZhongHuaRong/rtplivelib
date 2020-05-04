#pragma once

#include "abstractcapture.h"

namespace rtplivelib {

namespace device_manager {

class DesktopCapturePrivateData;

/**
 * @brief The DesktopCapture class
 * 该类负责桌面画面捕捉
 * 在类初始化后将会尝试打开设备
 * 如果打开设备失败则说明该系统不支持
 * 备注:
 * Windows系统
 * 底层采用GDI采集桌面，好像效率不太高，而且采集到的数据带有BMP结构头，
 * 大小为54字节,注意:通过接口获取到的数据是没有带数据头的，是纯数据
 * 只能采集主屏幕，不能截取特定窗口
 * set_window 用于截取特定的窗口，该接口暂时没有实现
 * WIN8以上的系统将采用DXGI捕捉,测试过性能比GDI要强很多
 * 现在默认采集的是BGRA32,以后会增加其他格式
 * 这将会减少一次转格式(系统自动将其他格式转为BGRA32),这个功能只限于WIN10
 * 所以采集的时候不转格式是效率最高的(因为后面要转一次YUV420或NV12用于编码)
 * 该方式捕捉到的桌面没有鼠标，等以后有空绘制鼠标
 * 
 * Linux系统
 * 底层采用fbdev采集桌面，目前采集到的是tty2，并不是桌面，以后有待研究
 * 图像格式是bgra32,在测试电脑里测试到只能跑15帧，跑不起30帧
 * 
 * 如果需要用该类自己处理数据，则可以考虑继承该类，然后重写on_frame_data接口来前处理数据
 * 然后返回false，也可以用wait_resource_push()和get_next()来获取队列的数据
 * 也可以通过实例化DeviceCapture类来捕捉，该类已经封装好其他四个类了，然后关闭其他三个的捕捉
 * 
 * 该类的枚举接口未完善，尽量不要使用接口枚举设备(因为毫无意义),目前只能采集默认设备
 * Win8以上系统采用的DXGI则可以设置采集设备(多屏采集),建议使用GPUDevice来枚举设备
 * 然后通过第一个显卡(默认输出的显卡)的screen_list数组的下标来set_current_device
 * 默认采集的是第一个显卡的第一个screen
 * set_current_device参数是string，需要将int转为string
 */
class RTPLIVELIBSHARED_EXPORT DesktopCapture :public AbstractCapture
{
public:
	DesktopCapture();
	
	~DesktopCapture() override;
	
	/**
	 * @brief set_fps
	 * 设置帧数，每秒捕捉多少张图片
	 * 该fps将会和camera的fps统一
	 * 以较小值为标准
	 * 这个函数会重新启动设备
	 * @param value
	 * 每秒帧数
	 */
	void set_fps(int value) noexcept;
	
	/**
	 * @brief get_fps
	 * 获取预设的帧数
	 */
	int get_fps() noexcept;
	
	/**
	 * @brief set_window
	 * 设置将要捕捉的窗口(暂未实现)
	 * @param id
	 * 窗口句柄，0为桌面
	 */
	void set_window(const uint64_t & id) noexcept;
	
	/**
	 * @brief get_all_device_info
	 * 获取所有设备的信息，其实也就是名字而已
	 * 这里说明一下，key是面向程序的，value是面向用户的
	 * key存的是设备id(外部调用可以忽略该字段)，value存的是设备名字(对用户友好)
	 * @return 
	 * 返回一个map
	 */
	virtual std::map<device_id,device_name> get_all_device_info() noexcept(false) override;
	
	/**
	 * @brief set_current_device
	 * 通过设备id更换当前设备
	 * 获取设备信息后，使用key来获取id，value是设备名字
	 * 如果失败则返回false
	 * @see get_all_device_info
	 */
	virtual bool set_current_device(device_id device_id) noexcept override;
	
	/**
	 * @brief set_default_device
	 * 将当前设备更换成默认设备,简易接口
	 * 该接口未实现
	 * @return 
	 */
	virtual bool set_default_device() noexcept override;
	
protected:
	/**
	 * @brief on_start
	 * 开始捕捉画面后的回调,实际开始捕捉数据的函数
	 */
	virtual SharedPacket on_start() noexcept override;
	
	/**
	 * @brief on_stop
	 * 结束捕捉画面后的回调，用于stop_capture后回收工作的函数
	 * 可以通过重写该函数来处理暂停后的事情
	 */
	virtual void on_stop() noexcept override;
	
	/**
	 * @brief open_device
	 * 尝试打开设备
	 * @return 
	 */
	bool open_device() noexcept;
private:
	int									_fps;
	uint64_t							_wid;
	DesktopCapturePrivateData * const	d_ptr;
};

inline int DesktopCapture::get_fps() noexcept									{		return _fps;}

} // namespace device_manager

} // namespace rtplivelib

