#pragma once

#include "abstractcapture.h"
#include <dxgi.h>
#include <dxgi1_2.h>
#include <dxgi1_5.h>
#include <d3d11.h>

namespace rtplivelib {

namespace device_manager {


/**
 * @brief The DXGICapture class
 * Windows系统下的桌面采集
 * 限于Win8以上系统，效率有待测试，应该比GDI好一点
 * 在Win8系统下捕捉到的图像格式是BGRA8888
 * Win10的话有可能是其他格式
 * 该类操作都是线程安全
 */
class DXGICapture :
		protected core::AbstractQueue<core::FramePacket>
{
public:
	//first:设备id
	//second:设备名字
	using device_id = std::wstring;
	using device_name = std::wstring;
	using device_info = std::pair<device_id,device_name>;
public:
	DXGICapture();
	
	virtual ~DXGICapture() override;
	
	/**
	 * @brief get_device_info
	 * 获取相应设备信息(GPU信息)
	 * @param ft
	 * 设备类型
	 * @return 
	 * 返回
	 */
	std::vector<device_info> get_all_device_info() noexcept(false);
	
	/**
	 * @brief get_current_device_info
	 * 获取当前设备id和名字
	 * 如果没有设置，则返回默认声卡信息
	 * @return 
	 */
	device_info get_current_device_info() noexcept;
	
	/**
	 * @brief set_current_device
	 * 通过索引更改当前使用的设备
	 * @param num
	 * 当前设备索引
	 * @param ft
	 * 设备类型
	 * @return 
	 */
	bool set_current_device(uint64_t num) noexcept;
	
	/**
	 * @brief set_current_device
	 * 通过设备id更改当前使用的设备
	 * 切换后需要手动调用start来开启设备
	 * @param id
	 * 设备id
	 * @param ft
	 * 设备类型
	 * @return 
	 */
	bool set_current_device(const device_id& id) noexcept;
	
	/**
	 * @brief set_default_device
	 * 设置默认的设备采集
	 * @param ft
	 * 设备类型,CARTURE或者RENDER
	 * @return 
	 */
	bool set_default_device() noexcept;
	
	/**
	 * @brief get_format
	 * 获取当前音频格式
	 * 需要开始采集才能获取
	 * @return 
	 */
	const core::Format get_format() noexcept;
	
	/**
	 * @brief start
	 * 开始采集,设置句柄，外部接口可以等待这个句柄来调用get_packet
	 * @return 
	 */
	bool start() noexcept;
	
	/**
	 * @brief stop
	 * 停止采集
	 * @return 
	 */
	bool stop() noexcept;
	
	/**
	 * @brief is_start
	 * 获取设备是否正在读取数据
	 * 如果正在读取则返回true
	 * @return 
	 */
	bool is_start() noexcept;
	
	/**
	 * @brief get_packet
	 * 获取音频包
	 * 该函数会阻塞当前线程,直到有包返回
	 */
	core::FramePacket::SharedPacket read_packet() noexcept;
protected:
	/**
	 * @brief on_thread_run
	 */
	virtual void on_thread_run() noexcept override final;
	
	/**
	 * @brief on_thread_pause
	 */
	virtual void on_thread_pause() noexcept override final;
	
	/**
	 * @brief get_thread_pause_condition
	 * @return 
	 */
	virtual bool get_thread_pause_condition() noexcept override final;
};

} // namespace device_manager

} // namespace rtplivelib
