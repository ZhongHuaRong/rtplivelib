
#pragma once

#include "../core/config.h"
#include "../core/globalcallback.h"
#include "rtpsendthread.h"
#include "rtpuser.h"
#include "rtppacket.h"
#include <mutex>
#include <list>
#include <memory>

namespace rtplivelib{

namespace rtp_network {

/**
 * @brief The RTPUserManager class
 * rtp会话用户管理
 * 用来管理发送源用户
 * 统一SSRC，用户名等信息
 * 由于要维护非推流用户太费资源，所以这里只统计推流用户
 */
class RTPLIVELIBSHARED_EXPORT RTPUserManager
{
public:
	using User = std::shared_ptr<RTPUser>;
public:
	/**
	 * @brief get_user_manager
	 * 获取全局的用户管理器
	 */
	static RTPUserManager * Get_user_manager() noexcept;
	
	/**
	 * @brief Release
	 * 析构全局的用户管理器
	 */
	static void Release() noexcept;
	
	/**
	 * @brief clear_all
	 * 清理所有用户
	 * 可以在退出房间时执行
	 */
	void clear_all() noexcept;
	
	/**
	 * @brief deal_with_rtp
	 * 处理rtp包
	 * @param packet
	 * RTP数据包，该包的所有控制权交给本类处理
	 * 不需要外部释放空间
	 * 所以，不能传栈上的对象做为参数
	 */
	void deal_with_rtp(RTPPacket::SharedRTPPacket rtp_packet) noexcept;
	
	/**
	 * @brief deal_with_rtcp
	 * 处理rtcp包，通过里面的信息来管理用户
	 */
	void deal_with_rtcp(void * p) noexcept;
	
	/**
	 * @brief get_user_count
	 * 获取用户数量
	 * 实际上是推流用户数量
	 * 遍历接口先不实现
	 */
	size_t get_user_count() noexcept;
	
	/**
	 * @brief get_all_users_name
	 * 获取列表里面所有用户的用户名
	 */
	const std::list<std::string> get_all_users_name() noexcept;
	
	/**
	 * @brief get_user
	 * 根据用户名获取user
	 * 返回true标示找到了
	 */
	bool get_user(const std::string & name,User & user) noexcept;
	
	/**
	 * 获取自己的音视频id,没开启会话则是0
	 */
	static uint32_t get_local_video_ssrc() noexcept;
	static uint32_t get_local_audio_ssrc() noexcept;
	
	/**
	 * @brief set_show_win_id
	 * 设置需要显示的id
	 */
	void set_show_win_id(void * id,const std::string & name) noexcept;
	
	/**
	 * @brief set_screen_size
	 * 设置显示的窗口的大小
	 */
	void set_screen_size(const std::string & name,
						 const int &win_w,const int & win_h,
						 const int & frame_w,const int & frame_h) noexcept;
protected:
	RTPUserManager();
	
	~RTPUserManager();
	
	/**
	 * @brief insert
	 * 添加ssrc,然后设置用户名
	 * @param ssrc
	 * 用户的ssrc
	 * @param name
	 * 用户名
	 * @return 
	 * 如果有添加新的或者已存在则返回true
	 * 如果没有添加则返回false
	 */
	bool insert(uint32_t ssrc,const std::string& name) noexcept;
	
	/**
	 * @brief remove
	 * 移除ssrc源，当一个用户的两个源都被移除后，该用户判定是退出会话
	 * 
	 * 如果成功移除一个用户则返回true
	 * 如果只是移除了一个用户中的其中一个源或其他情况则是false
	 * 
	 * @param reason
	 * 退出原因，其实可以忽略
	 * @param length
	 * 字符串的长度
	 */
	bool remove(uint32_t ssrc,const void *reason,const size_t& length) noexcept;
	
	/**
	 * @brief find
	 * 根据用户名查找相应的RTPUser
	 * 如果该用户不存在，则创建该用户，并设置用户名，然后返回
	 * 也就是说必定会返回一个RTPUser对象
	 */
	User find(const std::string& name) noexcept;
	
	/**
	 * @brief find
	 * find的重载函数
	 * 根据ssrc来查找相应的RTPUser
	 * 和之前的find不一样，这个是通过返回值来判断是否找到了
	 * 如果没有找到则是返回false
	 * 如果找到则返回true
	 * 如果两个ssrc都设置了则不移除,然后吧对应的ssrc置为0
	 * 否则从队列中移除该元素，同时也会吧ssrc置为0
	 * 说白了，这个重载是为了remove服务的
	 */
	bool find(const uint32_t & ssrc, User &user) noexcept;
	
	/**
	 * @brief get_user
	 * 根据ssrc获取user
	 * 返回true标示找到了
	 */
	bool get_user(const uint32_t & ssrc,User & user) noexcept;
	
	/**
	 * @brief deal_with_sdes
	 * 处理SDES字段信息
	 * @param sdes
	 * 数据
	 */
	void deal_with_sdes(void * sdes) noexcept;
	
	/**
	 * @brief set_active
	 * 设置进入房间的标志
	 */
	void set_active(bool flag) noexcept;
private:
	static RTPUserManager			*_manager;
	static volatile uint32_t		_local_video_ssrc;
	static volatile uint32_t		_local_audio_ssrc;
	std::list<User>					_user_list;
	std::mutex						_mutex;
	//判断自己是否进入房间
	volatile bool					_active;
	
	friend class RTPSendThread;
	friend class RtpSendThreadPrivateData;
};

inline RTPUserManager * RTPUserManager::Get_user_manager() noexcept				{
	if(_manager == nullptr){
		_manager = new RTPUserManager;
	}
	return _manager;
}
inline void RTPUserManager::Release() noexcept									{
	if(_manager != nullptr){
		delete _manager;
		_manager = nullptr;
	}
}
inline size_t RTPUserManager::get_user_count() noexcept							{
	return _user_list.size();
}
inline uint32_t RTPUserManager::get_local_video_ssrc() noexcept					{
	return _local_video_ssrc;
}
inline uint32_t RTPUserManager::get_local_audio_ssrc() noexcept					{
	return _local_audio_ssrc;
}
inline void RTPUserManager::clear_all() noexcept								{
	std::lock_guard<std::mutex> lk(_mutex);
	_user_list.clear();
}
inline void RTPUserManager::set_active(bool flag) noexcept						{
	_active = flag;
	if(flag == false)
		clear_all();
}

} // rtp_network

} // rtplivelib
