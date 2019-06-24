#include "wasapi.h"


namespace rtplivelib {

namespace device_manager {

WASAPI::WASAPI()
{
    
}

WASAPI::~WASAPI()
{
    
}

std::list<WASAPI::device_info> WASAPI::get_device_info(WASAPI::FlowType ft) noexcept(false)
{
    
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

}//namespace device_manager

}//namespace rtplivelib
