#include "audioprocessingfactory.h"
#include <iostream>
#include <algorithm>
extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavdevice/avdevice.h>
	#include <libavutil/dict.h>
}

namespace rtplivelib {

namespace device_manager {


////////////////////////////////////////////////////////////////////////////////////////////////////

SoundCardCapture::SoundCardCapture() :
	AbstractCapture(CaptureType::Soundcard)
{
    /*初始化步骤放到_init执行*/
	_initResult = _init();
	
	/*在子类初始化里开启线程*/
//	start_thread();
}

SoundCardCapture::~SoundCardCapture() 
{
	if (is_running()) {
		//析构的时候注意要先暂停，否则线程不会退出
		on_stop();
	}
#ifdef WIN32
	SafeRelease()(&pCaptureClient);
	SafeRelease()(&pAudioClient);
	SafeRelease()(&pDevice);
	SafeRelease()(&pEnumerator);

	if (eventHandle)
		CloseHandle(eventHandle);
#endif
}

std::map<std::string, std::string> SoundCardCapture::get_all_device_info() noexcept
{
	return std::map<std::string, std::string>();
}

bool SoundCardCapture::set_current_device_name(std::string name) noexcept
{
	UNUSED(name)
	return false;
}

/**
 * @brief _init
 * 用于初始化工作
 * @return
 * 初始化成功则返回true
 */
bool SoundCardCapture::_init() 
{
#ifdef WIN32
    /*这里的声卡采集，采用Windows的WASAPI*/
	pEnumerator = nullptr;
	pDevice = nullptr;
	pAudioClient = nullptr;
	pCaptureClient = nullptr;
	pwfx = nullptr;
	// 枚举设备
	auto hr = CoCreateInstance(CLSID_MMDeviceEnumerator,
		NULL,
		CLSCTX_ALL,
		IID_IMMDeviceEnumerator,
		(void**)&pEnumerator);
	if (FAILED(hr)) {
		std::cout << "Unable to CoCreateInstance" << std::endl;
		return false;
	}
	// 采集声卡
	hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
	if (FAILED(hr)) {
		std::cout << "Unable to GetDefaultAudioEndpoint" << std::endl;
		return false;
	}

	//创建一个管理对象，通过它可以获取到你需要的一切数据
	hr = pDevice->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&pAudioClient);
	if (FAILED(hr)) {
		std::cout << "Unable to Activate" << std::endl;
		return false;
	}

	//获取格式
	hr = pAudioClient->GetMixFormat(&pwfx);
	if (FAILED(hr)) {
		std::cout << "Unable to GetMixFormat" << std::endl;
		return false;
	}

	nFrameSize = (pwfx->wBitsPerSample / 8) * pwfx->nChannels;

	hr = pAudioClient->Initialize(
		AUDCLNT_SHAREMODE_SHARED,
		AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_LOOPBACK,
		10 * 10000, // 100纳秒为基本单位
		0,
		pwfx,
		NULL);
	if (FAILED(hr)) {
		std::cout << "Unable to GetDefaultAudioEndpoint" << std::endl;
		return false;
	}
	
#if _WIN32_WINNT >= 0x0600
	eventHandle = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
#else
	eventHandle = CreateEvent(NULL, false, false, nullptr);
#endif
	
	hr = pAudioClient->SetEventHandle(eventHandle);
	if (FAILED(hr))
	{
		printf("Unable to SetEventHandle");
		return false;
	}

	hr = pAudioClient->GetService(IID_IAudioCaptureClient, (void**)&pCaptureClient);
	if (FAILED(hr)) {
		std::cout << "Unable to GetService" << std::endl;
		return false;
	}
	return true;
#else
	return false;
#endif
}

/**
 * @brief OnStart
 * 开始捕捉音频后的回调
 */
AbstractCapture::SharedPacket SoundCardCapture::on_start()  noexcept
{
#if defined (WIN32)
//	if (!_initResult) {
//		_initResult = _init();
//		if(!_initResult){
//			std::cout << "Unable to Initialize" << std::endl;
//			stop_capture();
//			return nullptr;
//		}
//	}
//	auto hr = pAudioClient->Start();
//	if (FAILED(hr)) {
//		std::cout << "Unable to Start" << std::endl;
//		stop_capture();
//		return nullptr;
//	}

//	uint32_t packetLength;
//	unsigned char * pData;
//	uint32_t numFramesAvailable;
//	DWORD  flags;
	
//	//等待事件回调
//	WaitForSingleObject(eventHandle, 5);
	
//	pCaptureClient->GetNextPacketSize(&packetLength);

//	if (packetLength) {
//		pCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, nullptr, nullptr);
		
//		FramePacket * packet = new FramePacket();
//		packet->packet = nullptr;
//		packet->format.channels = pwfx->nChannels;
//		packet->format.bits = pwfx->cbSize;
//		packet->format.sample_rate = pwfx->nSamplesPerSec;
//		packet->format.format = pwfx->wFormatTag;
//		if (numFramesAvailable != 0)
//		{
//			//先计算大小，然后后面统一合成
//			auto size = numFramesAvailable * nFrameSize;
//			packet->data[0] = new uint8_t[size];
			
//			if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
//			{
//				//
//				//  Fill 0s from the capture buffer to the output buffer.
//				//
//				memset(packet->data[0],0,size);
//			}
//			else
//			{
//				//
//				//  Copy data from the audio engine buffer to the output buffer.
//				//
//				memcpy(packet->data[0],pData,size);
//			}
//		}
//		hr = pCaptureClient->ReleaseBuffer(numFramesAvailable);
//		return packet;
//	}
//	else
//		return nullptr;
	
	stop_capture();
	return nullptr;
#elif defined (unix)
	stop_capture();
	return nullptr;
#endif
}

/**
 * @brief OnStart
 * 结束捕捉音频后的回调
 */
void SoundCardCapture::on_stop() noexcept
{
    /*并没有任何操作，将标志位isrunning设为false就可以暂停了*/
#if defined (WIN32)
	pAudioClient->Stop();
	
	/*释放掉剩余没有提取的音频数据*/
	uint32_t packetLength;
	unsigned char * pData;
	uint32_t numFramesAvailable;
	DWORD  flags;
	
	pCaptureClient->GetNextPacketSize(&packetLength);

	//循环读取所有包,然后release
	while (packetLength) {
		pCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, nullptr, nullptr);
		
		pCaptureClient->ReleaseBuffer(numFramesAvailable);

		pCaptureClient->GetNextPacketSize(&packetLength);
	}
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////

AudioProcessingFactory::AudioProcessingFactory()
{
	
}

}// namespace device_manager

} // namespace rtplivelib 

