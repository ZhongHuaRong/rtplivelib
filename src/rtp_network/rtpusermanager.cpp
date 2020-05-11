#include "rtpusermanager.h"
#include "jrtplib3/rtppacket.h"
#include "jrtplib3/rtpsourcedata.h"
#include "jrtplib3/rtpaddress.h"
#include "jrtplib3/rtcpcompoundpacket.h"
#include "jrtplib3/rtcpapppacket.h"
#include "jrtplib3/rtcpsrpacket.h"
#include "jrtplib3/rtcprrpacket.h"
#include "jrtplib3/rtcpbyepacket.h"
#include "jrtplib3/rtcpsdespacket.h"
#include "jrtplib3/rtpipv4address.h"
#include "../core/logger.h"

namespace rtplivelib{

namespace rtp_network {

RTPUserManager * RTPUserManager::_manager = nullptr;
volatile uint32_t RTPUserManager::_local_video_ssrc = 0;
volatile uint32_t RTPUserManager::_local_audio_ssrc = 0;

void RTPUserManager::deal_with_rtp(RTPPacket::SharedRTPPacket rtp_packet) noexcept
{
	//分析数据
	jrtplib::RTPPacket * packet = static_cast<jrtplib::RTPPacket*>(rtp_packet->get_packet());
	//rtcp数据包不需要处理
	jrtplib::RTPSourceData * source = static_cast<jrtplib::RTPSourceData*>(rtp_packet->get_source_data());
	UNUSED(source)
			
	if(packet == nullptr){
		return;
	}
	auto && SSRC = packet->GetSSRC();
	
	User user(nullptr);
	
	if( get_user(SSRC,user) ){
		user->deal_with_packet(rtp_packet);
	}
}

void RTPUserManager::deal_with_rtcp(void *p) noexcept
{
	if( p == nullptr)
		return;
	
//	constexpr char api[] = "rtp_network::RTPUserManager::deal_with_rtcp";
	auto ptr = static_cast<jrtplib::RTCPCompoundPacket *>(p);
	jrtplib::RTCPPacket * rtcp_packet = nullptr;
	ptr->GotoFirstPacket();
	while( ( rtcp_packet = ptr->GetNextPacket() ) != nullptr ){
		
		switch(rtcp_packet->GetPacketType()){
		case jrtplib::RTCPPacket::PacketType::SR:
		{
//			auto packet = static_cast<jrtplib::RTCPSRPacket*>(rtcp_packet);
			//发送端报文先不解析，因为可能有多个发送端
//			core::Logger::Info("SR: send[{}],recv[{}],lost[{}]",
//							   api,
//							   packet->GetSenderPacketCount(),
//							   packet->GetFractionLost(packet-> GetReceptionReportCount() - 1 ),
//							   packet->GetLostPacketCount(packet-> GetReceptionReportCount() - 1 ));
			break;
		}
		case jrtplib::RTCPPacket::PacketType::RR:
		{
			auto packet = static_cast<jrtplib::RTCPRRPacket*>(rtcp_packet);
			if(core::GlobalCallBack::Get_CallBack() == nullptr)
				return;
			packet->GetLSR(packet-> GetReceptionReportCount() - 1 );
			const auto& fl = static_cast<float>(packet->GetFractionLost(packet-> GetReceptionReportCount() - 1 )) / 256.0f;
			const auto& jitter = packet->GetJitter(packet-> GetReceptionReportCount() - 1 );
			const auto& LSR = packet->GetLSR(packet-> GetReceptionReportCount() - 1 );
			auto DLSR = packet->GetDLSR(packet-> GetReceptionReportCount() - 1 );
			uint32_t rtt;
			if( LSR == 0 || DLSR == 0)
				rtt = 999999999;
			{
				const auto& recv_time = jrtplib::RTPTime::CurrentTime().GetNTPTime();
				rtt = ( (recv_time.GetMSW() << 16) & 0xFFFF0000 ) | ( (recv_time.GetLSW() >> 16) & 0xFFFF );
				rtt -= LSR;
				rtt -= DLSR;
				rtt = static_cast<uint32_t>( static_cast<double>(rtt) / 65536.0 * 1000);
			}
			core::GlobalCallBack::Get_CallBack()->on_local_network_information(jitter,fl,rtt);
			break;
		}
		case jrtplib::RTCPPacket::PacketType::BYE:
		{
			auto packet = static_cast<jrtplib::RTCPBYEPacket*>(rtcp_packet);
			auto && count = packet->GetSSRCCount();
			for( int n = 0; n < count; ++n){
				remove(packet->GetSSRC(n),packet->GetReasonData(),packet->GetReasonLength());
			}
			break;
		}
		case jrtplib::RTCPPacket::PacketType::SDES:
		{
			auto packet = static_cast<jrtplib::RTCPSDESPacket*>(rtcp_packet);
			if(!packet->GotoFirstChunk())
				break;
			do{
				deal_with_sdes(packet);
			}while(packet->GotoNextChunk());
			break;
		}
		//不处理APP数据包
		case jrtplib::RTCPPacket::PacketType::APP:
		default:
//				core::Logger::Info("unknown:[length:{}]",
//								   api,
//								   packet->GetPacketLength());
			break;
		}
	}
}

const std::list<std::string> RTPUserManager::get_all_users_name() noexcept
{
	std::list<std::string> list;
	std::lock_guard<std::mutex> lk(_mutex);
	for(auto name:_user_list){
		list.push_back(name->name);
	}
	return list;
}

bool RTPUserManager::get_user(const std::string &name, RTPUserManager::User &user) noexcept
{
	if(get_user_count() == 0)
		return false;
	std::lock_guard<std::mutex> lk(_mutex);
	auto it = _user_list.begin();
	for( ; it != _user_list.end(); ++it) {
		if( (*it)->name == name ){
			user = (*it);
			return true;
		}
	}
	return false;
}

void RTPUserManager::set_decoder_hwd_type(const std::string &name,
										  codec::HardwareDevice::HWDType type) noexcept
{
	User user(nullptr);
	if(get_user(name,user) == false)
		return;
	
	user->set_video_hwd_type(type);
}

void RTPUserManager::set_decoder_hwd_type(codec::HardwareDevice::HWDType type) noexcept
{
	_type = type;
	for(auto i:_user_list){
		i->set_video_hwd_type(type);
	}
}

std::map<std::string, codec::HardwareDevice::HWDType> 
RTPUserManager::get_decoder_hwd_type() noexcept
{
	std::map<std::string, codec::HardwareDevice::HWDType> map;
	
	for(auto i:_user_list){
		map[i->name] = i->_vdecoder.get_hwd_type();
	}
	
	return map;
}

codec::HardwareDevice::HWDType
RTPUserManager::get_decoder_hwd_type(const std::string &name) noexcept
{
	User user(nullptr);
	if(get_user(name,user) == true)
		return user->_vdecoder.get_hwd_type();
	
	return codec::HardwareDevice::None;
}

void RTPUserManager::set_show_win_id(void *id, const std::string &name) noexcept
{
	//没进入房间不处理
	if( _active == false)
		return;
	//没有名字不处理,因为名字是唯一标识
	//处理就是浪费时间
	if(name.size() == 0)
		return;

	//尝试搜索是否已经存在该用户名
	User user(nullptr);
	if(get_user(name,user) == true){
		user->set_win_id(id);
	}
}

void RTPUserManager::set_screen_size(const std::string &name,
									 const int &win_w, const int &win_h,
									 const int &frame_w, const int &frame_h) noexcept
{
	//没进入房间不处理
	if( _active == false)
		return;
	//没有名字不处理,因为名字是唯一标识
	//处理就是浪费时间
	if(name.size() == 0)
		return;

	//尝试搜索是否已经存在该用户名
	User user(nullptr);
	if(get_user(name,user) == true){
		user->set_display_screen_size(win_w,win_h,frame_w,frame_h);
	}
}

RTPUserManager::RTPUserManager()
{
	
}

RTPUserManager::~RTPUserManager()
{
	
}

bool RTPUserManager::insert(uint32_t ssrc, const std::string &name) noexcept
{
	//没进入房间不处理
	if( _active == false)
		return false;
	//没有名字不处理,因为名字是唯一标识
	//处理就是浪费时间
	if(name.size() == 0)
		return false;
	
	std::lock_guard<std::mutex> lk(_mutex);
	//尝试搜索是否已经存在该用户名
	auto && user = find(name);
	//提前设置硬解方案
	user->set_video_hwd_type(_type);
	//开始检查ssrc
	//这个是已经存在的用户了，不继续处理了
	if( ssrc == user->ssrc || ssrc == user->another_ssrc)
		return true;
	
	//设置其中的一个ssrc
	if( user->ssrc == 0)
		user->ssrc = ssrc;
	else if( user->another_ssrc == 0)
		user->another_ssrc = ssrc;
	else {
		//啥也没搞到,可能是出错了,记录一下
		//这个问题我考虑了一下，有可能是下面这种情况
		//在处于某种情况,推流那边在没有发送BYE包的情况下退出会话
		//然后重新登录，此时的SSRC已经改变，所以会出现这种情况，先不做处理
		core::Logger::Print_APP_Info(core::Result::Rtcp_insert_user_failed,
									 __PRETTY_FUNCTION__,
									 LogLevel::WARNING_LEVEL,
									 name,ssrc,
									 user->name,user->ssrc,user->another_ssrc);
		return false;
	}
	
	//这个是新添加的，而不是已存在的，所以需要回调一下
	//这里说明一下，在获取到其中一个源的时候就开启回调了
	//为了防止回调两次，所以在两个源都设置的时候不回调
	if( user->ssrc == 0 || user->another_ssrc == 0)
		if(core::GlobalCallBack::Get_CallBack() != nullptr)
			core::GlobalCallBack::Get_CallBack()->on_new_user_join(name);
	return true;
}

bool RTPUserManager::remove(uint32_t ssrc, const void *reason, const size_t &length) noexcept
{
	//没进入房间不处理
	if( _active == false)
		return false;
	//没有用户在列表不处理，浪费时间
	if(get_user_count() == 0)
		return false;
	//BYE包和推流标志都会调用该函数移除
	std::lock_guard<std::mutex> lk(_mutex);
	
	bool ret;
	User user;
	ret = find(ssrc,user);
	if(ret == false){
		//查找失败，应该是一开始并没有该元素
		return false;
	}
	else {
		//查找成功,并在find的时候移除了该元素
		if(core::GlobalCallBack::Get_CallBack() != nullptr)
			core::GlobalCallBack::Get_CallBack()->on_user_exit(user->name,reason,length);
		return true;
	}
}

RTPUserManager::User RTPUserManager::find(const std::string &name) noexcept
{
	auto it = _user_list.begin();
	for( ; it != _user_list.end(); ++it) {
		if( (*it)->name == name )
			return *it;
	}
	//找不到
	User user(new RTPUser);
	user->name = name;
	_user_list.push_back(user);
	return user;
}

bool RTPUserManager::find(const uint32_t & ssrc,User &user) noexcept
{
	auto it = _user_list.begin();
	while(it != _user_list.end()){
		//因为需要移除了两个字段才会是真的移除，所以需要判断两次
		if( (*it)->ssrc == 0 && (*it)->another_ssrc == 0){
			//如果一开始就两个ssrc都是0，好像是有点问题的
			//得记录一下,然后移除该异常数据
			core::Logger::Print_APP_Info(core::Result::Rtcp_remove_abnormal,
										 __PRETTY_FUNCTION__,
										 LogLevel::WARNING_LEVEL,
										 (*it)->name.c_str());
			auto i = it;
			++it;
			if(core::GlobalCallBack::Get_CallBack() != nullptr)
				core::GlobalCallBack::Get_CallBack()->on_user_exit((*i)->name,nullptr,0);
			_user_list.erase(i);
			continue;
		}
		if( (*it)->ssrc == ssrc){
			(*it)->ssrc = 0;
		}
		if( (*it)->another_ssrc == ssrc){
			(*it)->another_ssrc = 0;
		}
		//操作完成后，两个源都为0则是要移除他了
		if( (*it)->ssrc == 0 && (*it)->another_ssrc == 0){
			//如果一开始就两个ssrc都是0，好像是有点问题的
			//得记录一下
			user = (*it);
			_user_list.erase(it);
			return true;
		}
		++it;
	}
	
	return false;
}

bool RTPUserManager::get_user(const uint32_t & ssrc,User & user) noexcept 
{
	if(get_user_count() == 0)
		return false;
	std::lock_guard<std::mutex> lk(_mutex);
	auto it = _user_list.begin();
	for( ; it != _user_list.end(); ++it) {
		if( (*it)->ssrc == ssrc || (*it)->another_ssrc == ssrc ){
			user = (*it);
			return true;
		}
	}
	return false;
}

void RTPUserManager::deal_with_sdes(void *sdes) noexcept
{
	auto packet = static_cast<jrtplib::RTCPSDESPacket*>(sdes);
	
	if(!packet->GotoFirstItem())
		return;
	
	auto && ssrc = packet->GetChunkSSRC();
	std::string name,note;
	void * p;
	
	do{
		//先将指针值设置为指向空的指针
		//可以方便转为任意类型的指针类型
		p = packet->GetItemData();
		switch (packet->GetItemType()) {
		case jrtplib::RTCPSDESPacket::CNAME:
			//CNAME也不关心，因为不允许我设置
			break;
		case jrtplib::RTCPSDESPacket::NAME:
			name.append(static_cast<char*>(p),packet->GetItemLength());
			break;
		case jrtplib::RTCPSDESPacket::NOTE:
			note.append(static_cast<char*>(p),packet->GetItemLength());
			break;
		default:
			//其他字段不关心
			break;
		}
	}while(packet->GotoNextItem());
	
	//只存在加入
	if(name.size() < 1)
		return;
	insert(ssrc,name);
//	if( note.size() <= 1) {
//		//房间值都为0了，就是没加入房间，也没推流，没必要继续处理
//		//只有推流标志位也不行，也就是没有加入房间
//		return;
//	}
	
//	bool push = ( note[note.length() - 1] == '1' );
//	note.pop_back();
//	if(push){
//		//这个是要加入用户管理的
//		insert(ssrc,name);
//	}
//	else {
//		//这个是要移出用户管理的
//		remove(ssrc,nullptr,0);
//	}
}

} // rtp_network

} // rtplivelib
