#include "rtprecvthread.h"
#include "rtpusermanager.h"
#include "rtpbandwidth.h"
#include "../core/logger.h"
#include "jrtplib3/rtppacket.h"

namespace rtplivelib {

namespace rtp_network {

struct DownloadBW {
	void operator () (uint64_t speed,uint64_t total) {
		if(core::GlobalCallBack::Get_CallBack() != nullptr)
			core::GlobalCallBack::Get_CallBack()->on_download_bandwidth(speed,total);
	}
};

class RTPRecvThreadPrivateData {
public:
	RTPBandwidth bw;
	
	RTPRecvThreadPrivateData():
		bw(DownloadBW()){
		
	}
};

///////////////////////////////////////////////////////////////////////////////////

RTPRecvThread::RTPRecvThread():
	d_ptr(new RTPRecvThreadPrivateData)
{
//	set_max_size(65535);
	start_thread();
}

RTPRecvThread::~RTPRecvThread()
{
//	this->exit_wait_resource();
	exit_thread();
	delete d_ptr;
}

void RTPRecvThread::on_thread_run() noexcept
{
	//等待资源到来
	//100ms检查一次
//	this->wait_for_resource_push(100);
	
//	auto ptr = this->get_next();
//	if(ptr == nullptr)
//		return;
	
//	d_ptr->bw.add_value(static_cast<jrtplib::RTPPacket*>(ptr->get_packet())->GetPacketLength());
	
//	//统计一下流量，然后都扔给用户管理处理
//	RTPUserManager::Get_user_manager()->deal_with_rtp(ptr);
}


} // rtp_network

} // namespace rtplivelib
