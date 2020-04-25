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
 */
class DXGICapture:
        protected core::AbstractQueue<core::FramePacket>
{
public:
	using device_list = std::vector<GPUInfo>;
public:
	DXGICapture();
	
	virtual ~DXGICapture() override;
	
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
	 * @brief start
	 * 开始采集,
	 * @param time_space
	 * 时间间隔
	 * @return 
	 */
	bool start(int time_space) noexcept;
	
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
protected:
	/**
	 * @brief on_thread_run
	 */
	virtual void on_thread_run() noexcept override final;
	
	/**
	 * @brief get_thread_pause_condition
	 * @return 
	 */
	virtual bool get_thread_pause_condition() noexcept override final;
private:
	DXGIPrivate * const d_ptr;
	volatile bool _is_running_flag{false};
};

inline bool DXGICapture::get_thread_pause_condition() noexcept									{	return !_is_running_flag;}

} // namespace device_manager

} // namespace rtplivelib
