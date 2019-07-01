#include "wasapi.h"
#include "../core/except.h"
#include "../core/time.h"
#include <stdint.h>
extern "C"{
#include "libavutil/mem.h"
}

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

struct TaskMemFree {
	template<typename T>
	void operator () (T ** p) {
		if (*p != nullptr) {
			CoTaskMemFree(*p);
			*p = nullptr;
		}
	}
};

WASAPI::WASAPI()
{
	GUID IDevice_FriendlyName = { 0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0 } };
	key.pid = 14;
	key.fmtid = IDevice_FriendlyName;
#if _WIN32_WINNT >= 0x0600
	event = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
#else
	event = CreateEvent(nullptr, false, false, nullptr);
#endif
	
	start_thread();
}

WASAPI::~WASAPI()
{
    if(is_start()){
        stop();
        exit_thread();
    }
    SafeRelease()(&pEnumerator);
    SafeRelease()(&pDevice);
    SafeRelease()(&pAudioClient);
    SafeRelease()(&pCaptureClient);
    TaskMemFree()(&pwfx);
    if (event)
		CloseHandle(event);
}

std::vector<WASAPI::device_info> WASAPI::get_all_device_info(WASAPI::FlowType ft) noexcept(false)
{
    if( _init_enumerator() == false){
        throw core::uninitialized_error("IMMDeviceEnumerator");
    }
    
    IMMDeviceCollection * collenction{nullptr};
	auto hr = pEnumerator->EnumAudioEndpoints((EDataFlow)ft,DEVICE_STATE_ACTIVE,&collenction);
	if (FAILED(hr)) {
		throw core::uninitialized_error("IMMDeviceEnumerator");
	}
	
	uint32_t count;
	collenction->GetCount(&count);
	if(count == 0){
		SafeRelease()(&collenction);
		return std::vector<WASAPI::device_info>();
	}
	
	IMMDevice *pDevice{nullptr};
	std::vector<WASAPI::device_info> info_list;
	info_list.resize(count);
	for(auto n = 0u; n < count; ++n){
		
		hr = collenction->Item(n,&pDevice);
		if (FAILED(hr)) {
			SafeRelease()(&collenction);
			throw core::uninitialized_error("IMMDeviceEnumerator");
		}
		
		info_list.push_back( get_device_info(pDevice) );
		SafeRelease()(&pDevice);
	}
	SafeRelease()(&collenction);
	return info_list;
}

WASAPI::device_info WASAPI::get_current_device_info() noexcept
{
	if(pDevice == nullptr){
		if(set_default_device(WASAPI::FlowType::RENDER) == false){
			return device_info(L"",L"");
		}
	}
	
	return get_device_info(pDevice);
}

bool WASAPI::set_current_device(uint64_t num, WASAPI::FlowType ft) noexcept
{
    if( _init_enumerator() == false){
        return false;
    }
    try {
        auto list = get_all_device_info(ft);
        if( list.size() <= num )
            return false;
        
        set_current_device(list[num].first,ft);
    } catch (...) {
        return false;
    }
    return false;
}

bool WASAPI::set_current_device(const std::wstring &id, WASAPI::FlowType ft) noexcept
{
    UNUSED(ft)
    if( _init_enumerator() == false){
        return false;
    }
    IMMDevice *pDevice{nullptr};
    auto hr = pEnumerator->GetDevice(id.c_str(),&pDevice);
    if (FAILED(hr)) {
        return false;
    }
    
    //成功了再释放以前的设备
    stop();
    SafeRelease()(&this->pDevice);
    this->pDevice = pDevice;
    return true;
}

bool WASAPI::set_default_device(WASAPI::FlowType ft) noexcept
{
	if( _init_enumerator() == false){
        return false;
    }
    IMMDevice *pDevice{nullptr};
	auto hr = pEnumerator->GetDefaultAudioEndpoint((EDataFlow)ft, eConsole, &pDevice);
	if (FAILED(hr)) {
		return false;
	}
	
	//成功了再释放以前的设备
	stop();
	SafeRelease()(&this->pDevice);
    this->pDevice = pDevice;
    return true;
}

const core::Format WASAPI::get_format() noexcept
{
	core::Format format;
	if(pAudioClient == nullptr)
		return format;
	if(pwfx != nullptr){
		format.bits = pwfx->wBitsPerSample;
		format.channels = pwfx->nChannels;
		format.sample_rate = pwfx->nSamplesPerSec;
		return format;
	} else {
		auto hr = pAudioClient->GetMixFormat(&pwfx);
		if (FAILED(hr)) {
			return format;
		}
		format.bits = pwfx->wBitsPerSample;
		format.channels = pwfx->nChannels;
		format.sample_rate = pwfx->nSamplesPerSec;
		TaskMemFree()(&pwfx);
		return format;
	}
}

bool WASAPI::start() noexcept
{
	//如果没有设置，则设置默认的声卡采集
	if(pDevice == nullptr){
		if( set_default_device(RENDER)== false)
			return false;
	}
	
	SafeRelease()(&pCaptureClient);
	SafeRelease()(&pAudioClient);
	//创建一个管理对象，通过它可以获取到你需要的一切数据
	auto hr = pDevice->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, (void**)&pAudioClient);
	if (FAILED(hr)) {
		return false;
	}
	
	//获取格式
	TaskMemFree()(&pwfx);
	hr = pAudioClient->GetMixFormat(&pwfx);
	if (FAILED(hr)) {
		SafeRelease()(&pAudioClient);
		return false;
	}

	nFrameSize = (pwfx->wBitsPerSample / 8) * pwfx->nChannels;

	hr = pAudioClient->Initialize(
		AUDCLNT_SHAREMODE_SHARED,
		AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_LOOPBACK,
		50 * 10000, // 100纳秒为基本单位
		0,
		pwfx,
		nullptr);
	
	if (FAILED(hr)) {
		SafeRelease()(&pAudioClient);
		TaskMemFree()(&pwfx);
		return false;
	}
	
	pAudioClient->SetEventHandle(event);

	hr = pAudioClient->GetService(IID_IAudioCaptureClient, (void**)&pCaptureClient);
	if (FAILED(hr)) {
		SafeRelease()(&pAudioClient);
		TaskMemFree()(&pwfx);
		return false;
	}
	
	hr = pAudioClient->Start();
	if (FAILED(hr)) {
		return false;
	}
	
	_is_running_flag = true;
	notify_thread();
	return true;
}

bool WASAPI::stop() noexcept
{
	if(pAudioClient == nullptr)
		return true;
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
	
	SafeRelease()(&pCaptureClient);
	SafeRelease()(&pAudioClient);
	TaskMemFree()(&pwfx);
	_is_running_flag = false;
	return true;
}

bool WASAPI::is_start() noexcept
{
	return _is_running_flag;
}

core::FramePacket::SharedPacket WASAPI::read_packet() noexcept
{
	wait_resource_push();
	
	return get_next();
}

WASAPI::device_info WASAPI::get_device_info(IMMDevice *device) noexcept
{
	wchar_t * str{nullptr};
	PROPVARIANT varName;
	PropVariantInit(&varName);
	IPropertyStore * props{nullptr};
	
	device_info info;
	auto hr = device->GetId(&str);
	if (FAILED(hr)) {
		return info;
	}
	
	info.first = str;
	TaskMemFree()(&str);
	
	hr = device->OpenPropertyStore(STGM_READ,&props);
	if (FAILED(hr)) {
		return info;
	}
	
	props->GetValue(key,&varName);
	info.second = varName.pwszVal;
	SafeRelease()(&props);
	PropVariantClear(&varName);
	return info;
}

void WASAPI::on_thread_run() noexcept
{
	if(pAudioClient == nullptr){
		stop();
		return ;
	}
	
	WaitForSingleObject(event, 10);
	
	uint32_t packetLength;
	pCaptureClient->GetNextPacketSize(&packetLength);

	if (packetLength) {
		unsigned char * pData;
		uint32_t numFramesAvailable;
		DWORD  flags;
		pCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, nullptr, nullptr);
		
		if (numFramesAvailable != 0)
		{
			//先计算大小，然后后面统一合成
			auto size = numFramesAvailable * nFrameSize;
			
			auto packet = core::FramePacket::Make_Shared();
			if(packet == nullptr){
				pCaptureClient->ReleaseBuffer(numFramesAvailable);
				return ;
			}
			packet->data[0] = static_cast<uint8_t*>(av_malloc(size));
			if(packet->data[0] == nullptr){
				pCaptureClient->ReleaseBuffer(numFramesAvailable);
				return ;
			}
			
			if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
			{
				//
				//  Fill 0s from the capture buffer to the output buffer.
				//
				memset(packet->data[0],0,size);
			}
			else
			{
				//
				//  Copy data from the audio engine buffer to the output buffer.
				//
				memcpy(packet->data[0],pData,size);
			}
			pCaptureClient->ReleaseBuffer(numFramesAvailable);
			
			packet->size = size;
			packet->format = get_format();
			//获取时间戳
			packet->dts = core::Time::Now().to_timestamp();
			packet->pts = packet->dts;
			push_one(packet);
		}
	}
}

void WASAPI::on_thread_pause() noexcept
{
	stop();
}

bool WASAPI::_init_enumerator() noexcept
{
    if(pEnumerator)
        return true;
    
    auto hr = CoCreateInstance(CLSID_MMDeviceEnumerator,
                               nullptr,
                               CLSCTX_ALL,
                               IID_IMMDeviceEnumerator,
                               (void**)&pEnumerator);
    
    if(hr == 0)
        return true;
    else {
        return false;
    }
}

}//namespace device_manager

}//namespace rtplivelib
