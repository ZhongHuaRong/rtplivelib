
#ifndef AUDIOCAPTURE_H
#define AUDIOCAPTURE_H

#include "../capture/abstractcapture.h"
#include <stdint.h>
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <MMDeviceAPI.h>
#include <AudioClient.h>
#endif

class AVInputFormat;
class AVFormatContext;

namespace rtplivelib::device_manager::factory {

/**
 * @brief The MicrophoneCapture class
 * 该类负责麦克风音频捕捉
 * 在类初始化后将会尝试打开设备
 * 如果打开设备失败则说明该系统不支持
 * 备注:
 * Windows系统
 * 
 * Linux系统
 * 
 * 如果需要用该类自己处理数据，则可以考虑继承该类，然后重写on_frame_data接口来前处理数据
 * 然后返回false，也可以用wait_resource_push()和get_next()来获取队列的数据
 * 也可以通过实例化DeviceCapture类来捕捉，该类已经封装好其他四个类了，然后关闭其他三个的捕捉
 * @author 钟华荣
 * @date 2018-11-28
 */
class RTPLIVELIBSHARED_EXPORT MicrophoneCapture :
		public rtplivelib::device_manager::capture::AbstractCapture
{
public:
	MicrophoneCapture();

    ~MicrophoneCapture() override;
	
	virtual std::map<std::string,std::string> get_all_device_info() noexcept override;
	
	virtual bool set_current_device_name(std::string name) noexcept override;

protected:
    /**
     * @brief on_start
     * 开始捕捉音频后的回调
     */
	virtual FramePacket * on_start() override;

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
	AVInputFormat *ifmt;
	AVFormatContext *fmtContxt;
	std::mutex _fmt_ctx_mutex;
	std::string fmt_name;
};
    
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
 * @author 钟华荣
 * @date 2018-11-28
 */
class RTPLIVELIBSHARED_EXPORT SoundCardCapture :
		public rtplivelib::device_manager::capture::AbstractCapture
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
	virtual FramePacket * on_start() override;

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

/**
 * @brief The AudioCapture class
 * 该类统一音频处理
 * 将会把采集到的音频重采样(统一采样格式)，然后编码，格式为AAC
 * 该类也会提供原始音频数据获取接口，让外部调用可以做数据前处理
 */
class AudioProcessingFactory
{
public:
	AudioProcessingFactory();
};

} // factory

#endif // AUDIOCAPTURE_H
