#include "liveengine.h"
#include "codec/videoencoder.h"
#include "codec/audioencoder.h"
#include "rtp_network/rtpsession.h"
#include "rtp_network/rtpsendthread.h"
#include "rtp_network/rtprecvthread.h"
#include "rtp_network/rtpusermanager.h"
#include "core/logger.h"
#include "rtp_network/fec/codec/wirehair.h"
extern "C"{
#include "libavcodec/avcodec.h"
}
#ifdef WIN64
#include <winsock.h>
#endif

namespace rtplivelib {

class LiveEnginePrivateData {
public:
	codec::VideoEncoder * const video_encoder;
	codec::AudioEncoder * const audio_encoder;
	rtp_network::RTPSession * const video_session;
	rtp_network::RTPSession * const audio_session;
	rtp_network::RTPSendThread * const rtp_send;
	rtp_network::RTPRecvThread * const rtp_recv;
	rtp_network::RTPUserManager * const rtp_user;
	
	/**
	 * @brief LiveEnginePrivateData
	 * 初始化
	 */
	LiveEnginePrivateData():
		video_encoder(new codec::VideoEncoder()),
		audio_encoder(new codec::AudioEncoder()),
		video_session(new rtp_network::RTPSession),
		audio_session(new rtp_network::RTPSession),
		rtp_send(new rtp_network::RTPSendThread),
		rtp_recv(new rtp_network::RTPRecvThread),
		rtp_user(rtp_network::RTPUserManager::Get_user_manager())
	{
		
	}
	
	~LiveEnginePrivateData(){
		delete rtp_send;
		delete rtp_recv;
		delete video_encoder;
		delete audio_encoder;
		delete video_session;
		delete audio_session;
		
		rtp_network::RTPUserManager::Release();
	}
};

///////////////////////////////////////////////////////////////////////////////////

LiveEngine::LiveEngine():
	device(new device_manager::DeviceManager),
	d_ptr(new LiveEnginePrivateData)
{
	//初始化日志等级
	set_log_level(LogLevel::ALLINFO_LEVEL);
	//初始化Wirehair编解码器
	rtp_network::fec::Wirehair::InitCodec();
	
	//初始化socket
#ifdef WIN64
#ifdef RTP_SOCKETTYPE_WINSOCK
	WSADATA dat;
	WSAStartup(MAKEWORD(2, 2), &dat);
#endif
#endif
	
	/*在初始化的时候，关联所有类,让其可以正常工作*/
	//设置视频输入队列，输入队列为device的video_factory
	d_ptr->video_encoder->set_input_queue(device->get_video_factory());
	d_ptr->video_encoder->set_max_size(60);
	device->get_video_factory()->set_max_size(60);
	
	//设置音频输入队列，输入队列为device的audio_factory
	d_ptr->audio_encoder->set_input_queue(device->get_audio_factory());
	d_ptr->audio_encoder->set_max_size(30);
	
	//设置接口,只需要一个发送线程即可
	//因为RTPSession不是线程安全的，所以设置到发送线程RTPSendThread后
	//需要调用RTPSendThread的接口设置参数，而不能直接使用RTPSession类
	//视频设置
	d_ptr->rtp_send->set_video_session(d_ptr->video_session);
	d_ptr->rtp_send->set_video_send_queue(d_ptr->video_encoder);
	//音频设置
	d_ptr->rtp_send->set_audio_session(d_ptr->audio_session);
	d_ptr->rtp_send->set_audio_send_queue(d_ptr->audio_encoder);
	
	//AbstractQueue是线程安全的，所以只需要一个接收线程处理即可
	d_ptr->video_session->set_rtp_recv_object(d_ptr->rtp_recv);
	d_ptr->audio_session->set_rtp_recv_object(d_ptr->rtp_recv);
	
}

LiveEngine::~LiveEngine()
{
	d_ptr->video_encoder->set_input_queue(nullptr);
	d_ptr->audio_encoder->set_input_queue(nullptr);
	delete device;
	delete d_ptr;
	
	//在这里清除全局的日志模块
	core::Logger::Clear_all();
}

void LiveEngine::set_local_microphone_audio(bool flag) noexcept
{
	device->get_audio_factory()->play_microphone_audio(flag);
}

void LiveEngine::set_local_display_win_id(void *win_id)
{
	try {
		device->get_video_factory()->set_display_win_id(win_id);
	} catch (const std::bad_alloc& except) {
		core::Logger::Print(except.what(),
							__PRETTY_FUNCTION__,
							LogLevel::INFO_LEVEL);
	}
}

void LiveEngine::set_remote_display_win_id(void *win_id, const std::string &name)
{
	rtp_network::RTPUserManager::Get_user_manager()->set_show_win_id(win_id,name);
}

void LiveEngine::set_display_screen_size(const int &win_w, const int &win_h, 
										 const int &frame_w, const int &frame_h) noexcept
{
	device->get_video_factory()->set_display_screen_size(win_w,win_h,
														 frame_w,frame_h);
}

void LiveEngine::set_remote_display_screen_size(const std::string& name,
												const int &win_w, const int &win_h, 
												const int &frame_w, const int &frame_h) noexcept
{
	rtp_network::RTPUserManager::Get_user_manager()->set_screen_size(name,win_w,win_h,frame_w,frame_h);
}

void LiveEngine::set_crop_rect(const image_processing::Rect &rect)noexcept
{
	try {
		device->get_video_factory()->set_crop_rect(rect);
	} catch (const std::bad_alloc& except) {
		core::Logger::Print(except.what(),
							__PRETTY_FUNCTION__,
							LogLevel::INFO_LEVEL);
	}
}

void LiveEngine::set_overlay_rect(const image_processing::FRect &rect) noexcept
{
	device->get_video_factory()->set_overlay_rect(rect);
}

void LiveEngine::register_call_back_object(core::GlobalCallBack *callback) noexcept
{
	core::GlobalCallBack::Register_CallBack(callback);
}

bool LiveEngine::set_local_name(const std::string &name) noexcept
{
	return d_ptr->rtp_send->set_local_name(name);
}

bool LiveEngine::join_room(const std::string& name)noexcept 
{
	//如果没有设置用户名则不允许进入房间
	if(d_ptr->video_session->get_local_name().size() == 0)
		return false;
	if( get_room_name().size() != 0 ){
		//已经加入过房间
		exit_room();
	}
	
	return d_ptr->rtp_send->set_room_name(name);
}

bool LiveEngine::exit_room() noexcept
{
	return d_ptr->rtp_send->set_room_name("");
}

bool LiveEngine::enabled_push(bool enabled) noexcept
{
	return d_ptr->rtp_send->set_push_flag(enabled);
}

const std::string& LiveEngine::get_room_name() noexcept
{
	return d_ptr->video_session->get_room_name();
}

void *LiveEngine::get_audio_encoder() noexcept
{
	return d_ptr->audio_encoder;
}

void *LiveEngine::get_video_encoder() noexcept
{
	return d_ptr->video_encoder;
}

void LiveEngine::set_log_level(LogLevel level) noexcept
{
	core::Logger::log_set_level(level);
	
}

}
