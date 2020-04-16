#include "rtpuser.h"
#include <memory>
#include "jrtplib3/rtppacket.h"
#include "jrtplib3/rtpsourcedata.h"
#include "jrtplib3/rtpaddress.h"
#include "jrtplib3/rtcpcompoundpacket.h"
#include "jrtplib3/rtcpapppacket.h"
#include "jrtplib3/rtcpsdespacket.h"
#include "jrtplib3/rtpipv4address.h"
#include "../core/logger.h"
extern "C"{
#include "libavutil/mem.h"
}

namespace rtplivelib{

namespace rtp_network {

RTPUser::RTPUser()
{
	_vdecoder.set_player_object(&_vplay);
}

RTPUser::~RTPUser()
{
}

void RTPUser::deal_with_packet(RTPPacket::SharedRTPPacket rtp_packet) noexcept
{
	if(rtp_packet == nullptr)
		return;
	
	jrtplib::RTPPacket * packet = static_cast<jrtplib::RTPPacket*>(rtp_packet->get_packet());
	
	if( packet == nullptr)
		return;
	
	auto && pt = packet->GetPayloadType();
	auto && _pt = static_cast<RTPSession::PayloadType>(pt);
	if(packet->GetPayloadLength() == 0)
		return;
	
	fec::FECDecoder * fec_ptr;
	codec::VideoDecoder::Queue * decoder_ptr{nullptr};
	switch(pt){
	case RTPSession::PayloadType::RTP_PT_HEVC:
	case RTPSession::PayloadType::RTP_PT_H264:
	case RTPSession::PayloadType::RTP_PT_VP9:
		fec_ptr = &_vfecdecoder;
		decoder_ptr = &_vdecoder;
		break;
	case RTPSession::PayloadType::RTP_PT_AAC:
		fec_ptr = &_afecdecoder;
		decoder_ptr = &_adecoder;
		break;
	default:
		return;
	}
	
	if(fec_ptr->decode(rtp_packet) != core::Result::Success)
		return;
	decoder_ptr->push_one(std::make_shared<codec::VideoDecoder::Packet>(_pt,fec_ptr->get_packet()));
}

void RTPUser::set_win_id(void *id) noexcept
{
	_vplay.set_win_id(id);
}

void RTPUser::set_video_hwd_type(codec::HardwareDevice::HWDType type) noexcept
{
	_vdecoder.set_hwd_type(type);
}

codec::HardwareDevice::HWDType RTPUser::get_video_hwd_type() noexcept
{
	return _vdecoder.get_hwd_type();
}

void RTPUser::set_display_screen_size(const int &win_w, const int &win_h,
									  const int &frame_w, const int &frame_h) noexcept
{
	_vplay.show_screen_size_changed(win_w,win_h,frame_w,frame_h);
}

} // rtp_network

} // rtplivelib
