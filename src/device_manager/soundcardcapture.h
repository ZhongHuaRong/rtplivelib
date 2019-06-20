#pragma once

#include "abstractcapture.h"
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <MMDeviceAPI.h>
#include <AudioClient.h>
#endif

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
	
	virtual bool set_current_device_name(std::string name) noexcept override;

private:
    /**
     * @brief _init
     * 用于初始化工作
     * @return
     * 初始化成功则返回true
     */
	bool _init();

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

private:
	bool _initResult;

#ifdef WIN32
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
#endif
};

}//namespace device_manager

}//namespace rtplivelib
