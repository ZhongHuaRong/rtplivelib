
#pragma once

#include "../core/config.h"
#include "../core/taskthread.h"
#include "../core/format.h"
#define WIN32_LEAN_AND_MEAN
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <map>
#include <vector>

namespace rtplivelib {

namespace device_manager {

/**
 * @brief The SoundCardCapture class
 * Windows系统下的声音采集，分capture和render
 * 采用系统的WASAPI，不适用于XP系统，限于win7以上的系统
 * 该类操作都是线程安全
 */
class WASAPI :
		public core::TaskThread
{
public:
	enum FlowType{
		RENDER = 0,
		CAPTURE = 1,
		ALL = 2
	};
	
	//first:设备id
	//second:设备名字
	using device_id = std::wstring;
	using device_name = std::wstring;
	using device_info = std::pair<device_id,device_name>;
public:
	/**
	 * @brief WASAPI
	 */
	WASAPI();
	
	virtual ~WASAPI() override;
	
	/**
	 * @brief get_device_info
	 * 获取相应设备信息
	 * 这个接口在我测试的时候发现有一些设备名字无法获取
	 * 建议直接使用默认的设备
	 * @param ft
	 * 设备类型
	 * @return 
	 * 返回
	 */
	std::vector<device_info> get_all_device_info(FlowType ft = ALL) noexcept(false);
	
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
	bool set_current_device(uint64_t num,FlowType ft = ALL) noexcept;
	
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
	bool set_current_device(const device_id& id,FlowType ft = ALL) noexcept;
	
	/**
	 * @brief set_default_device
	 * 设置默认的设备采集
	 * @param ft
	 * 设备类型,CARTURE或者RENDER
	 * @return 
	 */
	bool set_default_device(FlowType ft = RENDER) noexcept;
	
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
protected:
	device_info get_device_info(IMMDevice * device) noexcept;
	
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
	/**
	 * @brief _init_enumerator
	 * 初始化枚举器
	 * @return 
	 */
	bool _init_enumerator() noexcept;
private:
	const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
	const IID   IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
	const IID   IID_IAudioClient = __uuidof(IAudioClient);
	const IID   IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);
	
	IMMDeviceEnumerator		*pEnumerator{nullptr};
	IMMDevice				*pDevice{nullptr};
	IAudioClient			*pAudioClient{nullptr};
	IAudioCaptureClient		*pCaptureClient{nullptr};
	WAVEFORMATEX			*pwfx{nullptr};
	HANDLE					event_handle{nullptr};
	uint32_t				nFrameSize{0u};
	PROPERTYKEY				key;
	HANDLE					event{nullptr};
	std::recursive_mutex	mutex;
	FlowType				current_flow_type{FlowType::RENDER};
	
	volatile bool _is_running_flag{false};
};

inline bool WASAPI::get_thread_pause_condition() noexcept									{	return !_is_running_flag;}

}//namespace device_manager

}//namespace rtplivelib
