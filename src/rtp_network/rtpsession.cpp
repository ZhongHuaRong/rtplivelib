#include "rtpsession.h"
#include "jrtplib3/rtpsession.h"
#include "jrtplib3/rtpudpv4transmitter.h"
#include "jrtplib3/rtpsessionparams.h"
#include "jrtplib3/rtpsourcedata.h"
#include "rtprecvthread.h"
#include "rtpusermanager.h"
#include "../core/logger.h"

namespace rtplivelib {

namespace rtp_network {

class RTPSessionPrivataData: public jrtplib::RTPSession{
public:
	rtp_network::RTPSession * obj;
	rtp_network::RTPRecvThread * recv_obj;
	
	RTPSessionPrivataData(rtp_network::RTPSession * object):
		obj(object),
		recv_obj(nullptr)
	{		}
	
	virtual ~RTPSessionPrivataData() override {
		
	}
	
	/**
	 * @brief set_room_name
	 * 推流标志和用户名绑定在一起
	 * 最有一位为1则是推流用户，为0则是拉流用户
	 * 不采用APP字段主要是为了主动发送RTCP包不好控制
	 * @return 
	 */
	inline int set_room_name() noexcept {
		std::string name = obj->_room_name;
		obj->_push_flag == true ? name.append("1"): name.append("0");
		if(IsActive())
			return SetLocalNote(name.c_str(),name.size());
		else
			return true;
	}
protected:
	virtual void OnValidatedRTPPacket(jrtplib::RTPSourceData *srcdat, jrtplib::RTPPacket *rtppack,
									  bool isonprobation, bool *ispackethandled) override {
		UNUSED(isonprobation)
		*ispackethandled = true;
		
		if(recv_obj == nullptr){
			delete rtppack;
			return;
		}
		//rtp包就交给接收线程处理
//		recv_obj->push_one(RTPPacket::Make_Shared(rtppack,srcdat));
	}
	
	virtual void OnRTCPCompoundPacket(jrtplib::RTCPCompoundPacket *pack,
									  const jrtplib::RTPTime &receivetime,
									  const jrtplib::RTPAddress *senderaddress) override {
		UNUSED(receivetime)
		UNUSED(senderaddress)
		//rtcp包就交给RTPUserManager处理,没有开线程
		//如果效果不好则考虑开线程
		rtp_network::RTPUserManager::Get_user_manager()->deal_with_rtcp(pack);
	}
private:
	/**
	 * @brief set_ip_from_Source
	 * 从Source中提取ip字符串和端口
	 * @param srcdat
	 * 需要提取的Source
	 * @param ip_str
	 * 需要设置的ip字符串指针
	 * @param port
	 * 需要设置的端口
	 */
	inline void set_ip_from_Source(const jrtplib::RTPSourceData * srcdat,
								   char *& ip_str,
								   uint16_t & port) noexcept {
		uint32_t ip;
		if (srcdat->GetRTPDataAddress() != nullptr)
		{
			const auto *addr = static_cast<const jrtplib::RTPIPv4Address *>(srcdat->GetRTPDataAddress());
			ip = addr->GetIP();
			port = addr->GetPort();
		}
		else if (srcdat->GetRTCPDataAddress() != nullptr)
		{
			const auto *addr = static_cast<const jrtplib::RTPIPv4Address *>(srcdat->GetRTCPDataAddress());
			ip = addr->GetIP();
			port = addr->GetPort() - 1;
		}
		else
			return;
		struct in_addr inaddr;
		inaddr.s_addr = htonl(ip);
		ip_str = inet_ntoa(inaddr);
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////

RTPSession::RTPSession():
	d_ptr(new RTPSessionPrivataData(this)),
	_push_flag(false)
{
}

RTPSession::~RTPSession()
{
	if(d_ptr->IsActive())
		d_ptr->BYEDestroy(jrtplib::RTPTime(10,0),nullptr,0);
	d_ptr->ClearDestinations();
	delete d_ptr;
}

int RTPSession::set_default_payload_type(const RTPSession::PayloadType &pt) noexcept
{
	return d_ptr->SetDefaultPayloadType(static_cast<uint8_t>(pt));
}

int RTPSession::set_default_mark(bool m) noexcept
{
	return d_ptr->SetDefaultMark(m);
}

int RTPSession::set_default_timestamp_increment(const uint32_t &timestampinc) noexcept
{
	return d_ptr->SetDefaultTimestampIncrement(timestampinc);
}

int RTPSession::set_maximum_packet_size(uint64_t s) noexcept
{
	return d_ptr->SetMaximumPacketSize(s);
}

int RTPSession::create(const double &timestamp_unit, const uint16_t &port_base) noexcept
{
	BYE_destroy(10,0,nullptr,0);
	
	jrtplib::RTPUDPv4TransmissionParams transparams;
	jrtplib::RTPSessionParams sessparams;
	//先设置默认值，到时候再根据实际情况设置
	sessparams.SetOwnTimestampUnit(timestamp_unit);
	sessparams.SetAcceptOwnPackets(true);
#if defined(SINGLEPORT)
	transparams.SetRTCPMultiplexing(true);
#endif
	transparams.SetPortbase(port_base);
	
	int ret = d_ptr->Create(sessparams,&transparams);
	//如果在创建会话之前设置过了用户名，则只是设置了参数
	//得在会话创建成功后，设置到会话中
	if(ret >= 0)
		set_local_name(_local_name);
	return ret;
}

bool RTPSession::is_active() noexcept
{
	return d_ptr->IsActive();
}

int RTPSession::add_destination(const uint8_t *ip, const uint16_t &port_base) noexcept
{
	//	return d_ptr->AddDestination(jrtplib::RTPIPv4Address(ip,port_base));
#ifdef SINGLEPORT
	return d_ptr->AddDestination(jrtplib::RTPIPv4Address(ip,port_base,port_base));
#else
	return d_ptr->AddDestination(jrtplib::RTPIPv4Address(ip,port_base));
#endif
}

int RTPSession::delete_destination(const uint8_t *ip, const uint16_t &port_base) noexcept
{
	//	return d_ptr->DeleteDestination(jrtplib::RTPIPv4Address(ip,port_base));
#ifdef SINGLEPORT
	return d_ptr->DeleteDestination(jrtplib::RTPIPv4Address(ip,port_base,port_base));
#else
	return d_ptr->DeleteDestination(jrtplib::RTPIPv4Address(ip,port_base));
#endif
}

void RTPSession::clear_destinations() noexcept
{
	d_ptr->ClearDestinations();
}

int RTPSession::send_packet(const void *data, const uint64_t &len) noexcept
{
	return d_ptr->SendPacket(data,len);
}

int RTPSession::send_packet(const void *data,const uint64_t &len,
							const uint8_t &pt,bool mark,const uint32_t &timestampinc) noexcept
{
	return d_ptr->SendPacket(data,len,pt,mark,timestampinc);
}

int RTPSession::send_packet_ex(const void *data, const uint64_t &len, 
							   const uint16_t &hdrextID,
							   const void *hdrextdata, const uint64_t &numhdrextwords) noexcept
{
	return d_ptr->SendPacketEx(data,len,
							   hdrextID,
							   hdrextdata,numhdrextwords);
}

int RTPSession::send_packet_ex(const void *data,const uint64_t &len,
							   const uint8_t &pt,bool mark,const uint32_t &timestampinc,
							   const uint16_t &hdrextID,
							   const void *hdrextdata,const uint64_t &numhdrextwords) noexcept
{
	return d_ptr->SendPacketEx(data,len,
							   pt,mark,timestampinc,
							   hdrextID,
							   hdrextdata,numhdrextwords);
}

int RTPSession::send_rtcp_app_packet(RTPSession::APPPacketType packet_type, const uint8_t name[],
									 const void *appdata, size_t appdatalen) noexcept
{
	return d_ptr->SendRTCPAPPPacket(packet_type,name,appdata,appdatalen);
}

int RTPSession::increment_timestamp_default() noexcept
{
	return d_ptr->IncrementTimestampDefault();
}

void RTPSession::BYE_destroy(const int64_t& max_time_seconds, const uint32_t & max_time_microseconds,
							 const void *reason, const uint64_t& reason_len) noexcept
{
	if(!d_ptr->IsActive())
		return;
	d_ptr->BYEDestroy(jrtplib::RTPTime(max_time_seconds,max_time_microseconds),reason,reason_len);
	//因为这个没有返回值，所以在类内发送日志
	core::Logger::Print_APP_Info(core::Result::Rtp_destroy_session,
								 __PRETTY_FUNCTION__,
								 LogLevel::INFO_LEVEL,
								 reason == nullptr?"no reason":static_cast<const char*>(reason));
}

int RTPSession::set_room_name(const std::string &name) noexcept
{
	d_ptr->SetNoteInterval(1);
	_room_name = name;
	return d_ptr->set_room_name();
}

int RTPSession::set_local_name(const std::string &name) noexcept
{
	d_ptr->SetNameInterval(1);
	_local_name = name;
	if(d_ptr->IsActive())
		return d_ptr->SetLocalName(_local_name.c_str(),_local_name.size());
	else
		return true;
}

uint32_t RTPSession::get_ssrc() noexcept
{
	return d_ptr->GetLocalSSRC();
}

int RTPSession::set_push_flag(const bool &flag) noexcept
{
	if(_push_flag == flag)
		return 0;
	_push_flag = flag;
	return d_ptr->set_room_name();
}

void RTPSession::set_rtp_recv_object(RTPRecvThread *object) noexcept
{
	d_ptr->recv_obj = object;
}

} // namespace rtp_network

} // namespace rtplivelib
