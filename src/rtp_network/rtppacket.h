
#pragma once

namespace rtplivelib {

namespace rtp_network {

/**
 * @brief The RTPPacket class
 * rtp数据包，只在内部使用
 * 只是暴露出来让模板成功编译
 */
class RTPPacket
{
public:
	/**
	 * @brief RTPPacket
	 * 构造函数，将会传入一个指针作为内部使用
	 * @param ptr
	 */
	RTPPacket(void * packet,
			  void * source_data);
	
	~RTPPacket();
	
	/**
	 * @brief get_object
	 * 获取内部包指针,有可能为空
	 */
	void * get_packet() noexcept;
	
	/**
	 * @brief get_source_data
	 * 获取资源数据,有可能为空
	 */
	void * get_source_data() noexcept;
private:
	void * packet;
	void * source_data;
};

inline void * RTPPacket::get_packet() noexcept							{		return packet;}
inline void * RTPPacket::get_source_data() noexcept						{		return source_data;}

} // namespace rtplivelib

} // rtp_network
