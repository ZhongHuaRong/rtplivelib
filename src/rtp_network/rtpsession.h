
#pragma once

#include "../core/config.h"
#include "../core/globalcallback.h"
#include <string>

namespace rtplivelib {

namespace rtp_network {

class RTPSessionPrivataData;
class RTPRecvThread;

/**
 * @brief The RTPSession class
 * RTP会话类，用于发送媒体信息和接收信息
 * 同时通过回调函数反馈网络信息
 * 该类的所有操作都不是线程安全
 */
class RTPLIVELIBSHARED_EXPORT RTPSession
{
public:
	/**
	 * @brief The PayloadType enum
	 * RTP会话中发送的有效载体类型，一般用来通知接收端媒体流格式
	 * 这个参数一定要设置，否则是0
	 */
	enum PayloadType{
		RTP_PT_NONE = 0,
		RTP_PT_HEVC = 96,
		RTP_PT_H264,
		RTP_PT_VP9,
		RTP_PT_AAC
	};
	
	/**
	 * @brief The APPPacketType enum
	 * rtcp协议发送的扩展字段
	 * 可以做app自己的信令
	 * 由于RTCP包发送的规则，所以暂时不使用该功能
	 */
	enum APPPacketType{
		RTP_APP_TYPE_UNKNOWN = 0,
		RTP_APP_TYPE_USER_JOIN,
		RTP_APP_TYPE_USER_EXIT
	};
	
public:
	/**
	 * @brief RTPSession
	 * 默认构造函数，将会使用提供的端口监听信息
	 * 该端口默认的，不需要手动设置
	 * @param port_base
	 */
	RTPSession();
	
	~RTPSession();
	
	/**
	 * @brief set_defaule_payload_type
	 * 设置默认的有效负载类型
	 */
	int set_default_payload_type(const PayloadType& pt) noexcept;
	
	/**
	 * @brief set_default_mark
	 * 设置默认的标记
	 */
	int set_default_mark(bool m) noexcept;
	
	/**
	 * @brief set_default_timestamp_increment
	 * 设置默认时间戳增量
	 */
	int set_default_timestamp_increment(const uint32_t &timestampinc) noexcept;
	
	/**
	 * @brief set_maximum_packet_size
	 * 设置最大的数据包大小
	 */
	int set_maximum_packet_size(uint64_t s) noexcept;
	
	/**
	 * @brief create
	 * 创建rtp会话
	 * @param timestamp_unit
	 * 时间戳单位，音频就是 1/采样率，视频就是1/帧率
	 * @param port_base
	 */
	int create(const double & timestamp_unit,const uint16_t & port_base) noexcept;
	
	/**
	 * @brief is_active
	 * 如果已经创建会话则返回true，否则返回false
	 */
	bool is_active() noexcept;
	
	/**
	 * @brief add_destination
	 * 增加目标地址，用于发送包的时候，发送到目标地址
	 * 可设置多个目标地址，可以通过delete_destination删除
	 * @param ip
	 * 目标地址
	 * @param port_base
	 * 端口
	 */
	int add_destination(const uint8_t *ip,
						const uint16_t &port_base) noexcept;
	
	/**
	 * @brief delete_destination
	 * 删除目标地址，之后发送的包就不会发送到该地址
	 * @param ip
	 * 需要移除的ip地址
	 * @param port_base
	 * 目标端口
	 */
	int delete_destination(const uint8_t *ip,
						   const uint16_t &port_base) noexcept;
	
	/**
	 * @brief clear_destinations
	 * 清除所有目标地址
	 */
	void clear_destinations() noexcept;
	
	/**
	 * @brief send_packet
	 * 发送数据,将会使用默认的有效负载类型，标志和时间戳增量
	 * @param data
	 * 需要发送的数据
	 * @param len
	 * 数据长度
	 */
	int send_packet(const void *data,const uint64_t &len) noexcept;
	
	/**
	 * @brief send_packet
	 * 发送数据，将会使用参数中的有效负载类型，标志和时间戳增量
	 * @param data
	 * 将要发送的数据
	 * @param len
	 * 数据长度
	 * @param pt
	 * 有效负载类型
	 * @param mark
	 * 标志
	 * @param timestampinc
	 * 时间戳增量
	 */
	int send_packet(const void *data,const uint64_t &len,
					const uint8_t &pt,bool mark,const uint32_t &timestampinc) noexcept;
	
	/**
	 * @brief send_packet_ex
	 * 发送数据的扩展版本，可以附带其他数据
	 * 这里将会使用默认的有效负载类型，标志和时间戳增量
	 * @param data
	 * 媒体数据
	 * @param len
	 * 媒体数据长度
	 * @param hdrextID
	 * 附带数据的id
	 * @param hdrextdata
	 * 附带数据
	 * @param numhdrextwords
	 * 附带数据的长度
	 */
	int send_packet_ex(const void *data,const uint64_t &len,
					   const uint16_t &hdrextID,
					   const void *hdrextdata,const uint64_t &numhdrextwords) noexcept;
	
	/**
	 * @brief send_packet_ex
	 * 发送数据的扩展版本，可以附带其他数据
	 * 这里将会使用参数中的有效负载类型，标志和时间戳增量
	 * @param data
	 * 媒体数据
	 * @param len
	 * 媒体数据长度
	 * @param pt
	 * 有效负载类型
	 * @param mark
	 * 标志
	 * @param timestampinc
	 * 时间戳增量
	 * @param hdrextID
	 * 附带数据的id
	 * @param hdrextdata
	 * 附带数据
	 * @param numhdrextwords
	 * 附带数据长度
	 */
	int send_packet_ex(const void *data,const uint64_t &len,
					   const uint8_t &pt,bool mark,const uint32_t &timestampinc,
					   const uint16_t &hdrextID,
					   const void *hdrextdata,const uint64_t &numhdrextwords) noexcept;
	
	/**
	 * @brief send_rtcp_app_packet
	 * 发送rtcp包，属于app字段的信令
	 * 由于该包发送由程序控制，所以不符合rtcp发送规则
	 * 需要谨慎使用该接口
	 * @param packet_type
	 * 字段类型
	 * @param name
	 * 名字
	 * @param appdata
	 * 需要发送的数据
	 * @param appdatalen
	 * 数据长度
	 * @return 
	 * 成功发送则返回非0
	 */
	int send_rtcp_app_packet(APPPacketType packet_type, const uint8_t name[4],
	const void *appdata, size_t appdatalen ) noexcept;
	
	/**
	 * @brief increment_timestamp_default
	 * 手动增加默认的时间戳增量
	 * 有时候需要静音，就不会发送数据，调用此函数可以有效的同步时间戳
	 */
	int increment_timestamp_default() noexcept;
	
	/**
	 * @brief BYE_destroy
	 * 发送给结束会话的信息，在要退出rtp会话时需要调用该函数
	 * @param max_time_seconds
	 * 等待的最大时间的秒数
	 * @param max_time_microseconds
	 * 等待的最大时间的毫秒数
	 * @param reason
	 * 离开的原因
	 * @param reason_len
	 * 长度
	 */
	void BYE_destroy(const int64_t& max_time_seconds, const uint32_t & max_time_microseconds,
					 const void *reason, const uint64_t& reason_len) noexcept;
	
	/**
	 * @brief set_cname
	 * 设置房间名字
	 */
	int set_room_name(const std::string& name) noexcept;
	
	/**
	 * @brief get_room_name
	 * 获取房间名字
	 */
	const std::string& get_room_name() noexcept;
	
	/**
	 * @brief set_local_name
	 * 设置在会话中的名字
	 */
	int set_local_name(const std::string & name) noexcept;
	
	/**
	 * @brief get_local_name
	 * 获取会话中的名字 
	 */
	const std::string& get_local_name() noexcept;
	
	/**
	 * @brief get_ssrc
	 * 获取SSRC
	 */
	uint32_t get_ssrc() noexcept;
	
	/**
	 * @brief set_push_flag
	 * 设置推流标志，用于判断这个用户是否需要推流
	 * 如果设置false,就算推流，目标地址也不会接受
	 * 如果设置true,就算不推流，目标地址也会在用户管理找到这个用户
	 * (用户管理只处理推流用户)
	 * @param flag
	 */
	int set_push_flag(const bool& flag) noexcept;
	
	/**
	 * @brief set_rtp_recv_object
	 * 设置rtp接收对象，用于专门处理接收到的rtp数据包
	 * 如果不需要处理接收的包，可以不设置该对象
	 */
	void set_rtp_recv_object(RTPRecvThread * object) noexcept;
private:
	RTPSessionPrivataData * const d_ptr;
	std::string _room_name;
	std::string _local_name;
	bool _push_flag;
	
	friend class RTPSessionPrivataData;
	friend class RTPRecvThread;
};

inline const std::string& RTPSession::get_room_name() noexcept									{
	return _room_name;
}

inline const std::string& RTPSession::get_local_name() noexcept									{
	return _local_name;
}


} // namespace rtp_network

} // namespace rtplivelib
