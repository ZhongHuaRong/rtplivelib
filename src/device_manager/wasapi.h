
#pragma once

#include "../core/config.h"
#include "../core/format.h"
#define WIN32_LEAN_AND_MEAN
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <map>
#include <list>

namespace rtplivelib {

namespace device_manager {

/*用于资源释放*/
struct SafeRelease {
	template<typename T>
	void operator () (T ** p) {
		if (*p != nullptr) {
			(*p)->Release();
			*p = nullptr;
		}
	}
};

/**
 * @brief The SoundCardCapture class
 * Windows系统下的声音采集，分capture和render
 * 采用系统的WASAPI，不适用于XP系统，限于win7以上的系统
 */
class WASAPI
{
public:
    enum FlowType{
        RENDER,
        CAPTURE,
        ALL
    };
    
    //first:设备id
    //second:设备名字
    using device_info = std::pair<std::wstring,std::wstring>;
public:
    /**
     * @brief WASAPI
     */
    WASAPI();
    
    ~WASAPI();
    
    /**
     * @brief get_device_info
     * 获取相应设备信息
     * 目前发现有一些设备名字无法获取
     * @param ft
     * 设备类型
     * @return 
     * 返回
     */
    std::list<device_info> get_device_info(FlowType ft = ALL) noexcept(false);
    
    /**
     * @brief set_current_device
     * 通过索引更改当前使用的设备
     * @param num
     * 当前设备索引
     * @param ft
     * 设备类型
     * @return 
     */
    bool set_current_device(int num,FlowType ft = ALL) noexcept;
    
    /**
     * @brief set_current_device
     * 通过设备id更改当前使用的设备
     * @param id
     * 设备id
     * @param ft
     * 设备类型
     * @return 
     */
    bool set_current_device(const std::wstring& id,FlowType ft = ALL) noexcept;
    
    /**
     * @brief set_default_device
     * 设置默认的设备采集
     * @param ft
     * 设备类型,CARTURE或者RENDER
     * @return 
     */
    bool set_default_device(FlowType ft = RENDER) noexcept;
    
    /**
     * @brief set_event_handle
     * 设置事件句柄,通过等待事件来间隔获取音频
     * @param handle
     * 事件句柄
     * @return 
     */
    bool set_event_handle(HANDLE handle) noexcept;
    
    /**
     * @brief get_format
     * 获取当前音频格式
     * @return 
     */
    const core::Format get_format() noexcept;
    
    /**
     * @brief start
     * 开始采集
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
     * @brief get_packet
     * 获取音频包
     */
    core::FramePacket * get_packet() noexcept;
private:
    bool _init_enumerator() noexcept;
private:
	const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
	const IID   IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
	const IID   IID_IAudioClient = __uuidof(IAudioClient);
	const IID   IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);

	IMMDeviceEnumerator *pEnumerator;
	IMMDevice           *pDevice;
	IAudioClient        *pAudioClient;
	IAudioCaptureClient *pCaptureClient;
	WAVEFORMATEX        *pwfx;
	HANDLE				eventHandle;
	uint32_t			nFrameSize;
};

}//namespace device_manager

}//namespace rtplivelib
