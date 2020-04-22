#pragma once

#include "abstractcapture.h"
#include "gpudevice.h"

namespace rtplivelib {

namespace device_manager {

class DXGIPrivate;

/**
 * @brief The DXGICapture class
 * Windows系统下的桌面采集
 * 限于Win8以上系统，效率有待测试，应该比GDI好一点
 * 在Win8系统下捕捉到的图像格式是BGRA8888
 * Win10的话有可能是其他格式
 * 该类不另外开线程采集数据，和WASAPI不同，音频是需要时刻采集数据
 * 所以需要开线程获取
 * 图像的话要求不高，交给上级(DesktopCapture)控制采集
 */
class DXGICapture
{
public:
	using device_list = std::vector<GPUInfo>;
public:
	DXGICapture();
	
	virtual ~DXGICapture();
	
	/**
	 * @brief get_device_info
	 * 获取所有设备信息(GPU信息和对应的显示器信息)
	 * @param latest_flag
	 * 该标志用于表示是否获取最新的设备信息
	 * 为了更有效率获取设备信息，这里不会动态更新信息
	 * 当设备变更的时候需要传入true来获取更新后的设备信息
	 * @return 
	 * 返回
	 */
	device_list get_all_device_info(bool latest_flag = false) noexcept;
	
	/**
	 * @brief get_current_device_info
	 * 获取当前设备信息
	 * @param latest_falg
	 * see get_all_device_info
	 * @return 
	 * 该接口返回的只有一个GPU信息和对应的显示器信息
	 * 而不是输出所有显示器信息
	 */
	GPUInfo get_current_device_info(bool latest_flag = false) noexcept;
	
	/**
	 * @brief get_current_index
	 * 获取当前设置的GPU索引和显示器索引
	 * 配合get_all_device_info可以获取到当前设备信息
	 * @return 
	 * first:GPU index
	 * second:screen index
	 */
	std::pair<int,int> get_current_index() noexcept;
	
	/**
	 * @brief set_current_device
	 * 通过索引更改当前使用的设备
	 * @param gpu_num
	 * GPU索引,固定为0，目前不支持其他设置
	 * @param screen_num
	 * 显示器索引
	 * @return 
	 */
	bool set_current_device(int gpu_num,int screen_num) noexcept;
	
	/**
	 * @brief set_default_device
	 * 设置默认的设备采集
	 * @return 
	 */
	bool set_default_device() noexcept;
	
	/**
	 * @brief get_packet
	 * 获取数据
	 * 数据包的时间戳没设置，外部调用根据自己的时间间隔来设置时间戳
	 */
	core::FramePacket::SharedPacket read_packet() noexcept;
private:
	DXGIPrivate * const d_ptr;
};

} // namespace device_manager

} // namespace rtplivelib
