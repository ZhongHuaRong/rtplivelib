#include "wasapi.h"
#include "../core/except.h"
#include <stdint.h>

namespace rtplivelib {

namespace device_manager {

WASAPI::WASAPI()
{
	GUID IDevice_FriendlyName = { 0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0 } };
	key.pid = 14;
	key.fmtid = IDevice_FriendlyName;
}

WASAPI::~WASAPI()
{
    SafeRelease()(&pEnumerator);
}

std::list<WASAPI::device_info> WASAPI::get_device_info(WASAPI::FlowType ft) noexcept(false)
{
    if( _init_enumerator() == false){
        throw core::uninitialized_error("IMMDeviceEnumerator");
    }
    
    IMMDeviceCollection * collenction{nullptr};
	auto hr = pEnumerator->EnumAudioEndpoints(eAll,DEVICE_STATE_ACTIVE,&collenction);
	if (FAILED(hr)) {
		throw core::uninitialized_error("IMMDeviceEnumerator");
	}
	
	uint32_t count;
	collenction->GetCount(&count);
	if(count == 0){
		SafeRelease()(&collenction);
		return std::list<WASAPI::device_info>();
	}
	wchar_t * str{nullptr};
	PROPVARIANT varName;
	PropVariantInit(&varName);
	IPropertyStore * props{nullptr};
	std::list<WASAPI::device_info> info_list;
	for(auto n = 0u; n < count; ++n){
		collenction->Item(n,&pDevice);
		pDevice->GetId(&str);
		pDevice->OpenPropertyStore(STGM_READ,&props);
		device_info info(str,varName.pwszVal);
		info_list.push_back(device_info(str,varName.pwszVal));
		CoTaskMemFree(str);
		str = nullptr;
		SafeRelease()(&pDevice);
		SafeRelease()(&props);
	}
	PropVariantClear(&varName);
	return info_list;
}

bool WASAPI::set_current_device(int num, WASAPI::FlowType ft) noexcept
{
    
}

bool WASAPI::set_current_device(const std::wstring &id, WASAPI::FlowType ft) noexcept
{
    
}

bool WASAPI::set_default_device(WASAPI::FlowType ft) noexcept
{
    
}

bool WASAPI::set_event_handle(HANDLE handle) noexcept
{
    
}

const core::Format WASAPI::get_format() noexcept
{
    
}

core::FramePacket *WASAPI::get_packet() noexcept
{
    
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
