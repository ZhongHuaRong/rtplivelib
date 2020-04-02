#include "fecdecoder.h"
#include "fecencoder.h"
#include "../../core/logger.h"
#include "codec/wirehair.h"
#include "../rtpsession.h"
#include "jrtplib3/rtppacket.h"
#include "jrtplib3/rtpsourcedata.h"
#include "jrtplib3/rtpaddress.h"
#include "jrtplib3/rtcpcompoundpacket.h"
#include "jrtplib3/rtcpapppacket.h"
#include "jrtplib3/rtcpsdespacket.h"
#include "jrtplib3/rtpipv4address.h"

namespace rtplivelib {

namespace rtp_network {

namespace fec {

class FECDecoderPrivateData{
public:
    /*编码器*/
    Wirehair codec;
    
    RTPSession::PayloadType pt;
    
    FECDecoderPrivateData():
        codec(Wirehair::Decoder){
        
    }
    
    inline uint32_t get_correct_timestamp(const uint32_t &ts, const uint16_t &pos) noexcept
    {
        return ts - pos;
    }
    
    inline core::Result pop(void *data, const uint64_t &len,
                            const uint32_t &timestamp, 
                            const uint16_t &src_nb,
                            const uint16_t &repair_nb, 
                            const uint16_t &fill_size,
                            const uint16_t &pos,
                            const int &payload_type,
                            RTPPacket::SharedRTPPacket &rtp_packet) noexcept{
        
    }
   
};

///////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////


FECDecoder::FECDecoder():
    d_ptr(new FECDecoderPrivateData)
{
    
}

FECDecoder::~FECDecoder()
{
    delete d_ptr;
}

core::Result FECDecoder::decode(RTPPacket::SharedRTPPacket rtp_packet) noexcept
{
    jrtplib::RTPPacket * packet = static_cast<jrtplib::RTPPacket*>(rtp_packet->get_packet());
	bool ret;
	if(packet->GetExtensionData() != nullptr){
		FECParam param;
		memcpy(&param,packet->GetExtensionData(),sizeof(FECParam));
		ret = pop(packet->GetPayloadData(),packet->GetPayloadLength(),
				  packet->GetTimestamp(),
				  static_cast<uint16_t>( (extension_data >> 48 ) & 0x000000000000FFFF ),
				  static_cast<uint16_t>( (extension_data >> 32 ) & 0x000000000000FFFF ),
				  static_cast<uint16_t>( extension_data & 0x000000000000FFFF ),
				  packet->GetExtensionID(),
				  packet->GetPayloadType(),
				  rtp_packet);
	}
	else
		ret = pop(packet->GetPayloadData(),packet->GetPayloadLength(),
				  packet->GetTimestamp(),
				  1,
				  0,
				  0,
				  0,
				  packet->GetPayloadType(),
				  rtp_packet);
	
	if(ret == true)
		return get_correct_timestamp(packet->GetTimestamp(),packet->GetExtensionID());
	else
		return 0;
}

} //namespace fec

} //namespace rtp_network

} //namespace rtplivelib

