#include "rtpsendthread.h"
#include "../core/logger.h"
#include "../core/time.h"
#include "rtpbandwidth.h"
#include "rtpusermanager.h"
#include "./fec/fecencoder.h"
#include "jrtplib3/rtpsession.h"
#include "jrtplib3/rtpudpv4transmitter.h"
#include "jrtplib3/rtpsessionparams.h"
extern "C" {
#include <libavformat/avformat.h>
}

namespace rtplivelib{

namespace rtp_network{

//回调函数设计失败，太过耦合，需要重新设计
struct BandwidthCB {
	void operator () (uint64_t speed,uint64_t total) {
		if(core::GlobalCallBack::Get_CallBack() != nullptr)
			core::GlobalCallBack::Get_CallBack()->on_upload_bandwidth(speed,total);
	}
};

class RtpSendThreadPrivateData {
public:
	//用户保存音频采样率,要设置时间增长幅度
	int latest_sample_rate;
	//用于保存上一次发送的编码格式
	int latest_video_pt;
	int latest_audio_pt;
	//这个主要是用来获取rtp会话
	RTPSendThread * object{nullptr};
	//统计上传流量
	RTPBandwidth bandwidth;
	//用于FEC编码
	fec::FECEncoder fec_encoder;
	
	/**
	 * @brief RtpSendThreadPrivateData
	 * 用来设置RtpSendThread对象
	 */
	RtpSendThreadPrivateData(RTPSendThread * obj):
		object(obj),
		bandwidth(BandwidthCB())
	{		
		fec_encoder.set_symbol_size(RTPPACKET_MAX_SIZE);
	}
	
	/**
	 * @brief check_format
	 * 检查当前格式，当检查到的格式出现变化时，将会更改rtp的设置
	 * 当需要增加一个功能(根据网络情况自动更换合适的视频格式的时候)
	 * 这个接口就非常好用
	 * @param payload_type
	 * 有效负载类型，用于填充RTP协议字段，同时便于解码
	 * @param latest_pt
	 * 上一次保存的有效负载类型
	 * @param sample_rate
	 * 只用于音频，修改rtp字段
	 * @param is_video
	 * 是否是视频格式
	 * true:视频
	 * false:音频
	 */
	void check_format(int payload_type,
					  int sample_rate,
					  bool is_video) noexcept {
		if( is_video == true){
			if( latest_video_pt == payload_type)
				return;
			else
				//设置最新格式
				latest_video_pt = payload_type;
		}
		else {
			if( latest_audio_pt == payload_type && sample_rate == latest_sample_rate)
				return;
			else {
				//设置最新格式
				latest_sample_rate = sample_rate;
				latest_audio_pt = payload_type;
			}
		}
		
		const char *type = is_video == true?"video":"audio";
		auto & session = is_video == true ? object->_video_session:
											object->_audio_session;
		
		auto && pt = static_cast<RTPSession::PayloadType>(payload_type);
		auto && ret = session->set_default_payload_type(pt);
		if(ret < 0){
			core::Logger::Print_APP_Info(core::Result::Rtp_set_payload_type_failed,
										 __PRETTY_FUNCTION__,
										 LogLevel::WARNING_LEVEL,
										 type);
			core::Logger::Print_RTP_Info(ret,
										 __PRETTY_FUNCTION__,
										 LogLevel::WARNING_LEVEL);
		}
		ret = session->set_default_mark(false);
		if(ret < 0){
			core::Logger::Print_APP_Info(core::Result::Rtp_set_mark_failed,
										 __PRETTY_FUNCTION__,
										 LogLevel::WARNING_LEVEL);
			core::Logger::Print_RTP_Info(ret,
										 __PRETTY_FUNCTION__,
										 LogLevel::WARNING_LEVEL);
		}
		//不知道怎么算这个数
		if(is_video){
			//			auto fps = format.frame_rate == 0 ? 15u : static_cast<uint32_t>(format.frame_rate);
			ret = session->set_default_timestamp_increment( 1 );
		}
		else {
			auto sample_rate = latest_sample_rate == 0 ? 8000u : static_cast<uint32_t>(latest_sample_rate);
			//不知道怎么算这个数
			ret = session->set_default_timestamp_increment( 1u / sample_rate );
		}
		if(ret < 0){
			core::Logger::Print_APP_Info(core::Result::Rtp_set_timestamp_increment_failed,
										 __PRETTY_FUNCTION__,
										 LogLevel::WARNING_LEVEL,
										 type);
			core::Logger::Print_RTP_Info(ret,
										 __PRETTY_FUNCTION__,
										 LogLevel::WARNING_LEVEL);
		}
	}
	
	/**
	 * @brief init_session
	 * 初始化rtp会话,创建会话
	 * @param session
	 * rtp会话对象
	 * @param port_base
	 * 监听端口
	 * @param timestampUnit
	 * 时间戳增量
	 * @param is_video
	 * 视频或者音频
	 */
	void init_session(RTPSession *session,
					  const uint16_t & port_base,
					  const double & timestampUnit,
					  bool is_video) noexcept {
		int ret;
		
		ret = session->create(timestampUnit,port_base);
		//设置最大发送包的大小,不过好像分包还是要自己实现
		session->set_maximum_packet_size(65535u);
		const char* type = is_video? "video":"audio";
		if(ret < 0){
			core::Logger::Print_APP_Info(core::Result::Rtp_set_timestamp_increment_failed,
										 __PRETTY_FUNCTION__,
										 LogLevel::WARNING_LEVEL,
										 type,
										 port_base);
			core::Logger::Print_RTP_Info(ret,
										 __PRETTY_FUNCTION__,
										 LogLevel::WARNING_LEVEL);
		}
		else {
			core::Logger::Print_APP_Info(core::Result::Rtp_listening_port_base_success,
										 __PRETTY_FUNCTION__,
										 LogLevel::INFO_LEVEL,
										 type,
										 port_base);
			//设置本地SSRC
			if(is_video)
				RTPUserManager::_local_video_ssrc = session->get_ssrc();
			else
				RTPUserManager::_local_audio_ssrc = session->get_ssrc();
			//创建完成后，设置服务器ip和端口
			if(is_video)
				set_destination(SERVER_IP,VIDEO_PORTBASE,session,type);
			else
				set_destination(SERVER_IP,AUDIO_PORTBASE,session,type);
		}
	}
	
	/**
	 * @brief exit_session
	 * 离开会话
	 * @param session
	 * rtp会话对象
	 * @param reason
	 * 离开的原因
	 * @param reason_len
	 * 长度
	 * @param is_video
	 * 视频或者音频
	 */
	void exit_session(RTPSession *session,
					  const char * reason,const size_t& reason_len,
					  bool is_video) noexcept{
		
		session->BYE_destroy(10,0,reason,reason_len);
		//设置本地SSRC
		if(is_video)
			RTPUserManager::_local_video_ssrc = 0;
		else
			RTPUserManager::_local_audio_ssrc = 0;
	}
	
	/**
	 * @brief set_destination
	 * RtpSendThread::set_destination的具体实现
	 * @param ip
	 * 目标ip
	 * @param port_base
	 * 目标端口
	 * @param session
	 * rtp会话
	 * @param is_video
	 * 视频或者音频
	 */
	void set_destination(const uint8_t *ip,
						 const uint16_t &port_base,
						 RTPSession *session,
						 const char *type) noexcept{
		
		//先把原来的链接断掉
		session->clear_destinations();
		
		auto && ret = session->add_destination(ip, port_base);
		if(ret < 0){
			core::Logger::Print_APP_Info(core::Result::Rtp_create_destination_failed,
										 __PRETTY_FUNCTION__,
										 LogLevel::WARNING_LEVEL,
										 type,
										 ip[0],ip[1],ip[2],ip[3],port_base);
			core::Logger::Print_RTP_Info(ret,
										 __PRETTY_FUNCTION__,
										 LogLevel::WARNING_LEVEL);
		}
		else
			core::Logger::Print_APP_Info(core::Result::Rtp_create_destination_success,
										 __PRETTY_FUNCTION__,
										 LogLevel::INFO_LEVEL,
										 type,
										 ip[0],ip[1],ip[2],ip[3],port_base);
	}
	
	/**
	 * @brief send_packet
	 * 发送包的具体实现
	 * @param packet
	 * 将要发送的数据包
	 * @param is_video
	 * 发送的类型,音频或者视频
	 * true为视频
	 * false为音频
	 */
	void send_packet(core::FramePacket::SharedPacket packet,
					 bool is_video) noexcept {
		
		auto & session = is_video == true ? object->_video_session:
											object->_audio_session;
		
		std::vector<std::vector<int8_t>> data;
		fec::FECParam param;
		if( fec_encoder.encode(packet,data,param) != core::Result::Success) {
			core::Logger::Print_APP_Info(core::Result::FEC_Encode_Failed,
										 __PRETTY_FUNCTION__,
										 LogLevel::WARNING_LEVEL);
			
			//编码失败后，直接发送
			param.repair_nb = 0;
			param.flag = 0;
			auto src_nb = param.size / param.symbol_size;
			if(src_nb * param.symbol_size != param.size)
				++src_nb;
			auto _d = packet->data[0];
			uint16_t cur_pos = 0u;
			
			for( ; cur_pos < src_nb - 1; ++cur_pos ){
				_send_packet_ex(session,
								_d + cur_pos * param.symbol_size,
								fec_encoder.get_symbol_size(),
								cur_pos,
								param);
			}
			
			//发送最后一个包
			_send_packet_ex(session,
							_d + cur_pos * param.symbol_size,
							param.size % param.symbol_size,
							cur_pos,
							param);
		} else {
			//分包处理
			//分包策略是，id为当前包位置
			//然后发送的数据固定8字节，高4位中数据高二位是源数据包数，数据低二位是冗余包数
			//低4位中,数据高二位是最后一个包填充字节数,低二位保留
			//16bit,65535个包数，够用了
			for( uint16_t cur_nb = 0u; cur_nb < data.size(); ++ cur_nb){
				_send_packet_ex(session,
								data[cur_nb].data(),
								data[cur_nb].size(),
								cur_nb,
								param);
			}
		}
		
	}
	
private:
	void _send_packet_ex(RTPSession * session,void *d,uint32_t size,uint16_t cur_pos,fec::FECParam param) noexcept{
		auto ret = session->send_packet_ex( d, size,cur_pos,&param,sizeof(fec::FECParam));
		
		if( ret < 0 ){
			core::Logger::Print_APP_Info(core::Result::Rtp_send_packet_failed,
										 __PRETTY_FUNCTION__,
										 LogLevel::WARNING_LEVEL);
			core::Logger::Print_RTP_Info(ret,
										 __PRETTY_FUNCTION__,
										 LogLevel::WARNING_LEVEL);
		}
		else 
			bandwidth.add_value(fec_encoder.get_symbol_size());
	}
	
};

////////////////////////////////////////////////////////////////////////////////////////////////

RTPSendThread::RTPSendThread():
	_video_queue(nullptr),
	_audio_queue(nullptr),
	_video_session(nullptr),
	_audio_session(nullptr),
	d_ptr(new RtpSendThreadPrivateData(this))
{
	start_thread();
}

RTPSendThread::~RTPSendThread()
{
	exit_thread();
	if(_video_session != nullptr){
		d_ptr->exit_session(_video_session,nullptr,0,true);
	}
	if(_audio_session !=  nullptr){
		d_ptr->exit_session(_audio_session,nullptr,0,false);
	}
}

void RTPSendThread::set_video_session(RTPSession *video_session) noexcept
{
	std::lock_guard<decltype(_mutex)> lk(_mutex);
	//如果之前设置了一个会话，则先吧之前的会话关闭
	if(_video_session != nullptr){
		d_ptr->exit_session(_video_session,nullptr,0,true);
	}
	_video_session = video_session;
	if(video_session == nullptr)
		return;
	
	this->notify_thread();
}

void RTPSendThread::set_audio_session(RTPSession *audio_session) noexcept
{
	std::lock_guard<decltype(_mutex)> lk(_mutex);
	//如果之前设置了一个会话，则先吧之前的会话关闭
	if(_audio_session != nullptr){
		d_ptr->exit_session(_audio_session,nullptr,0,false);
	}
	_audio_session = audio_session;
	if(audio_session == nullptr)
		return;
	this->notify_thread();
}

void RTPSendThread::send_video_packet(core::FramePacket::SharedPacket packet)
{
	if(packet == nullptr)
		return;
	std::lock_guard<decltype(_mutex)> lk(_mutex);
	//如果会话没有设置或者建立或者没加入房间，都不处理
	if(_video_session == nullptr){
		return;
	}
	if(!_video_session->is_active() || _video_session->get_room_name().size() == 0){
		return;
	}
	d_ptr->check_format(packet->payload_type,packet->format.sample_rate,true);
	d_ptr->send_packet(packet,true);
	
}

void RTPSendThread::send_audio_packet(core::FramePacket::SharedPacket packet)
{
	if(packet == nullptr)
		return;
	std::lock_guard<decltype(_mutex)> lk(_mutex);
	//如果会话没有设置或者建立或者没加入房间，都不处理
	if(_audio_session == nullptr){
		return;
	}
	if(!_audio_session->is_active() || _audio_session->get_room_name().size() == 0)
		return;
	d_ptr->check_format(packet->payload_type,packet->format.sample_rate,false);
	d_ptr->send_packet(packet,false);
}

void RTPSendThread::set_destination( const uint8_t *ip, uint16_t port_base) noexcept
{
	std::lock_guard<decltype(_mutex)> lk(_mutex);
	
	if(_video_session == nullptr){
		//没有设置会话
		return;
	}
	else
		d_ptr->set_destination(ip,port_base,_video_session,"video");
	
	if(_audio_session == nullptr){
		//没有设置会话
		return;
	}
	else
		d_ptr->set_destination(ip,port_base + 2,_audio_session,"audio");
}

bool RTPSendThread::set_local_name(const std::string &name) noexcept
{
	std::lock_guard<decltype(_mutex)> lk(_mutex);
	int ret_v{0},ret_a{0};
	if(_video_session != nullptr){
		ret_v = _video_session->set_local_name(name);
	}
	if(_audio_session != nullptr){
		ret_a = _audio_session->set_local_name(name);
	}
	if(ret_v < 0)
		core::Logger::Print_RTP_Info(ret_v,
									 __PRETTY_FUNCTION__,
									 LogLevel::WARNING_LEVEL);
	if(ret_a < 0)
		core::Logger::Print_RTP_Info(ret_a,
									 __PRETTY_FUNCTION__,
									 LogLevel::WARNING_LEVEL);
	return ret_v >= 0 && ret_a >=0;
}

bool RTPSendThread::set_room_name(const std::string &name) noexcept
{
	//在设置房间名的时候就设置好用户管理的标志位
	RTPUserManager::Get_user_manager()->set_active(name.size() != 0);
	std::lock_guard<decltype(_mutex)> lk(_mutex);
	//这个是退出房间
	if(name.size() == 0){
		constexpr char reason[] = "exit room";
		if(_video_session != nullptr )
			d_ptr->exit_session(_video_session,reason,sizeof(reason),true);
		if(_audio_session != nullptr)
			d_ptr->exit_session(_audio_session,reason,sizeof(reason),true);
	}
	//加入房间
	else {
		//使用服务器的时候就是随机端口
		//如果是从一个房间换到另一个房间，这里也不会出现问题
		//因为会话在创建的时候会关闭之前的会话
		
		if(_video_session != nullptr)
			d_ptr->init_session(_video_session,
								0,
								1.0 / 15.0,
								true);
		if(_audio_session != nullptr)
			d_ptr->init_session(_audio_session,
								0,
								1.0 / 8000.0,
								false);
	}
	
	int ret_v{0},ret_a{0};
	if(_video_session != nullptr){
		ret_v = _video_session->set_room_name(name);
	}
	if(_audio_session != nullptr){
		ret_a = _audio_session->set_room_name(name);
	}
	if(ret_v < 0)
		core::Logger::Print_RTP_Info(ret_v,
									 __PRETTY_FUNCTION__,
									 LogLevel::WARNING_LEVEL);
	if(ret_a < 0)
		core::Logger::Print_RTP_Info(ret_a,
									 __PRETTY_FUNCTION__,
									 LogLevel::WARNING_LEVEL);
	return ret_v >= 0 && ret_a >=0;
}

bool RTPSendThread::set_push_flag(bool flag) noexcept
{
	int ret_v{0},ret_a{0};
	if(_video_session != nullptr){
		ret_v = _video_session->set_push_flag(flag);
	}
	if(_audio_session != nullptr){
		ret_a = _audio_session->set_push_flag(flag);
	}
	if(ret_v < 0)
		core::Logger::Print_RTP_Info(ret_v,
									 __PRETTY_FUNCTION__,
									 LogLevel::WARNING_LEVEL);
	if(ret_a < 0)
		core::Logger::Print_RTP_Info(ret_a,
									 __PRETTY_FUNCTION__,
									 LogLevel::WARNING_LEVEL);
	return ret_v >= 0 && ret_a >=0;
}

void RTPSendThread::on_thread_run() noexcept
{
	//让出时间片，因为可能会一直占用着锁
	//sleep(0)让时间片的方式还是很难让其他线程抢夺锁
	sleep(1);
	std::lock_guard<decltype(_mutex)> lk(_mutex);
	if(_video_queue != nullptr){
		//暂停一下下,否则一直跑，cpu都吃光光
		//不过这里的处理不够严谨，毕竟还是会吃到cpu
		if(_video_queue->wait_for_resource_push( 1 ) == true ){
			auto video_packet = _video_queue->get_next();
			if(video_packet != nullptr){
				//获取到视频编码帧
				send_video_packet(video_packet);
			}
		}
	}
	if(_audio_queue != nullptr){
		if(_audio_queue->wait_for_resource_push( 1 ) == true ){
			auto audio_packet = _audio_queue->get_next();
			if(audio_packet != nullptr){
				//获取到音频编码帧
				send_audio_packet(audio_packet);
			}
		}
	}
}


} // namespace rtp_network

} // namespace rtplivelib
