#include "rtppacket.h"
#include "jrtplib3/rtppacket.h"
#include "jrtplib3/rtpsourcedata.h"


namespace rtplivelib {

namespace rtp_network {

/*该类属于再次封装jrtplib::RTPPacket,在代码内部使用
 *同时能够让模板编译成功*/

RTPPacket::RTPPacket(void *packet,
					 void * source_data):
	packet(packet),
	source_data(source_data)
{
}

RTPPacket::~RTPPacket()
{
	if(packet != nullptr){
		auto p = static_cast<jrtplib::RTPPacket*>(packet);
		delete p;
	}
}

} // namespace rtplivelib

} // rtp_network

