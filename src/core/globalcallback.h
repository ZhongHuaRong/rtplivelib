#pragma once

#include <stdint.h>
#include <string>
#include <map>
#include "format.h"

namespace rtplivelib {

namespace core {

/**
 * @brief The MediaDataCallBack class
 * 这个是用来回调该lib的所有回调函数
 * 回调函数应该尽可能的简单，否则可能会影响lib的运行
 */
class GlobalCallBack
{
public:
	GlobalCallBack() = default;
	
	virtual ~GlobalCallBack(){}
	
	/**
	 * @brief on_camera_frame
	 * 摄像头画面帧的回调,该帧是没有经过任何处理的
	 * @param frame
	 */
	virtual void on_camera_frame(core::FramePacket::SharedPacket frame);
	
	/**
	 * @brief on_desktop_frame
	 * 桌面画面帧的回调,该帧是没有经过任何处理的
	 * @param frame
	 */
	virtual void on_desktop_frame(core::FramePacket::SharedPacket frame);

	/**
	 * @brief on_video_frame_merge
	 * 桌面以及摄像头画面合成后的图像帧回调
	 * @param frame
	 */
	virtual void on_video_frame_merge(core::FramePacket::SharedPacket frame);
	
	/**
	 * @brief on_microphone_packet
	 * 麦克风的音频包回调
	 * @param packet
	 */
	virtual void on_microphone_packet(core::FramePacket::SharedPacket packet);
	
	/**
	 * @brief on_soundcard_packet
	 * 声卡采集的音频包回调
	 * @param packet
	 */
	virtual void on_soundcard_packet(core::FramePacket::SharedPacket packet);
	
	/*以下回调继承的时候业务不要写的太复杂，会干扰rtp包或者rtcp包的接收和处理*/
	
	/**
	 * @brief on_new_user_join
	 * 有新的用户加入到房间
	 * 可能是推流的人进房间了，也可能是已经在房间里面，但是没有推流的用户开始推流了
	 * @param name
	 * 只有名字字段
	 */
	virtual void on_new_user_join(const std::string& name);
	
	/**
	 * @brief on_user_exit
	 * 有用户退出推流，并不是离开房间
	 * 如果reason不为空则是退出房间
	 * @param name
	 * 用户名
	 * @param reason
	 * 退出的原因
	 */
	virtual void on_user_exit(const std::string& name,
							  const void * reason, const uint64_t& reason_len);
	
	/**
	 * @brief on_upload_bandwidth
	 * 上传占用的带宽,只是统计媒体数据所占用的带宽，没有统计协议带来的头结构的开销
	 * 所以统计出来的会比实际上要少
	 */
	virtual void on_upload_bandwidth(uint64_t speed,uint64_t total);
	
	/**
	 * @brief on_user_bandwidth
	 * 下载带宽占用回调,实际的下载情况，和上传不同
	 */
	virtual void on_download_bandwidth(uint64_t speed,uint64_t total);
	
	/**
	 * @brief on_local_network_information
	 * 回调自己的网络信息
	 * @param jitter
	 * 抖动
	 * @param fraction_lost
	 * 丢包率
	 * @param delay
	 * 网络延迟
	 */
	virtual void on_local_network_information(uint32_t jitter,
											  float fraction_lost,
											  uint32_t delay) ;
	
	/**
	 * @brief Register_CallBack
	 * 注册回调函数
	 * 一个程序只需要一个回调
	 * @return 
	 */
	static void Register_CallBack(GlobalCallBack *) noexcept;
	
	static GlobalCallBack * Get_CallBack() noexcept;
private:
	static GlobalCallBack * cb_ptr;
};

inline void GlobalCallBack::on_camera_frame(core::FramePacket::SharedPacket )						{}
inline void GlobalCallBack::on_desktop_frame(core::FramePacket::SharedPacket )						{}
inline void GlobalCallBack::on_video_frame_merge(core::FramePacket::SharedPacket )					{}
inline void GlobalCallBack::on_microphone_packet(core::FramePacket::SharedPacket )					{}
inline void GlobalCallBack::on_soundcard_packet(core::FramePacket::SharedPacket )					{}
inline void GlobalCallBack::on_new_user_join(const std::string& )									{}
inline void GlobalCallBack::on_user_exit(const std::string& ,
											const void *, const uint64_t& )							{}
inline void GlobalCallBack::on_upload_bandwidth(uint64_t ,uint64_t)									{}
inline void GlobalCallBack::on_download_bandwidth(uint64_t,uint64_t)								{}
inline void GlobalCallBack::on_local_network_information(uint32_t ,float,uint32_t  )				{}

inline void GlobalCallBack::Register_CallBack(GlobalCallBack * ptr) noexcept				{
	cb_ptr = ptr;
}

inline GlobalCallBack *GlobalCallBack::Get_CallBack() noexcept							{
	return cb_ptr;
}


}// namespace core

} // namespace rtplivelib
