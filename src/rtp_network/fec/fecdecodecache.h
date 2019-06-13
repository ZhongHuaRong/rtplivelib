#pragma once

#include "../../core/config.h"
#include "../rtppacket.h"
#include "../rtpsession.h"
#include "../../core/timer.h"
#include <map>

namespace rtplivelib {

namespace rtp_network {

namespace fec {

/**
 * @brief The Data struct
 * 保存最底层的数据包
 */
struct PackageData{
	/*保存底层数据包*/
	RTPPacket * packet{nullptr};
	/*保存数据指针，懒得再从RTPPacket里面获取了*/
	void *data{nullptr};
	/*数据长度，懒得从RTPPacket里面获取了*/
	uint16_t len{0};
	
	PackageData() = default;
	
	PackageData(const PackageData&) = delete;
	
	~PackageData(){
		if(packet != nullptr)
			delete  packet;
	}
	
	inline void set_data(void *data,const uint64_t &len,RTPPacket * pkt){
//		if(packet != nullptr)
//			delete  packet;
		this->data = data;
		this->len = len;
		packet = pkt;
	}
};

/**
 * @brief The Package struct
 * 缓存集合，用于排序
 */
struct PackageVector {
	/**
	 * 存放Data的数据
	 * 采用数组的方式是因为比较适合随机访问
	 */
	PackageData ** list{nullptr};
	
	/**
	 * 时间戳
	 * 用于判断分包和组包
	 */
	uint32_t ts{0};
	
	/**
	 * 原数据包数
	 * 当保存的包数大于等于该参数时，将会解包成功
	 */
	uint16_t src_nb{0};
	
	/**
	 * 冗余包数
	 * 用于计算数组长度
	 */
	uint16_t repair_nb{0};
	
	/**
	 * 已经保存的包的数量
	 */
	uint32_t save_nb{0};
	
	/**
	 * 填充的字节数
	 */
	uint16_t fill_size{0};
	
	/**
	 * 是否含有冗余包的标志
	 * 该标志用于判断是否需要使用算法算出原数据
	 * 如果 save_nb == src_nb 时,而该标志又为false时
	 * 可以不调用算法,节省时间
	 */
	bool not_have_repair{true};
	
	PackageVector() = default;
	
	~PackageVector();
	
	/**
	 * @brief release_vector
	 * 释放内部数组，同时重置内部参数
	 */
	void release_vector() noexcept;
	
	/**
	 * @brief init_vector
	 * 初始化内部数组
	 * @param ts
	 * 用于初始化时间戳
	 * @param src_nb
	 * 计算原包数量
	 * @param repair_nb
	 * 计算冗余包数量
	 * @param fill_size
	 * 填充字节数,初始化参数
	 * @return 
	 * 当初始化失败时，返回false
	 * 出现这种情况就比较尴尬，啥事都干不了
	 */
	bool init_vector(const uint32_t &ts,const uint16_t& src_nb,const uint16_t &repair_nb,const uint16_t& fill_size) noexcept;
	
	/**
	 * @brief insert_data
	 * 添加一个数据包
	 */
	void insert_data(void *data, const uint64_t &len,
					 const uint16_t &pos, 
					 RTPPacket *packet) noexcept;
	
	/**
	 * @brief is_finish
	 * 判断是否可以进行解码了
	 * @return 
	 * 可以解码或者源数据包完整无缺则返回true
	 */
	inline bool is_finish() noexcept {
		return save_nb >= src_nb;
	}
	
	/**
	 * @brief get_block_size
	 * 获取块大小
	 * @return 
	 */
	uint16_t get_block_size() noexcept;
	
	/**
	 * @brief copy_and_combination
	 * 拷贝一份数据到连续的内存空间
	 * 将二维数组的数据组成一维，连接起来
	 */
	static std::pair<void*,uint64_t> copy_data(void ** ptr,const uint16_t &total_nb, 
											   const uint16_t& block_size,const uint16_t& fill_size) noexcept;
	
	/**
	 * @brief copy_data
	 * 从PackageVector里面拷贝数据出来
	 * @return 
	 */
	static std::pair<void*,uint64_t> copy_data(PackageVector *vector) noexcept;
	
	/**
	 * @brief create_array
	 * 创建两个指向void的二维数组
	 * 数据来自于list,所以不需要手动外部释放空间
	 * 只需要删除指针所占用的空间就可以了
	 * free(返回值)
	 * @return 
	 * first:保存的是解码前的数据，数据来自于list,所以不需要手动外部释放空间，只需要删除指针所占用的空间就可以了
	 * second:保存的是解码成功后的所有原数据,只需要释放掉first没有的和指针所占用的空间就可以了
	 * 或者调用release_decode_array接口释放
	 */
	std::pair<void**,void**> create_array() noexcept;
	
	/**
	 * @brief release_decode_array
	 * 释放create_array所生成的空间
	 * @param pair
	 */
	void release_decode_array(std::pair<void**,void**> pair) noexcept;
};

/**
 * @brief The FECDecodeCache class
 * 用于解码时保存分包数据的缓存
 * 每次通过pop添加分包，然后通过返回值判断是否有包符合条件解压
 * 这里需要注意的一点就是,当一个包符合条件解压或者原包没有丢失,
 * 而他的时间戳之前还有其他包时,将会丢弃以前的包
 */
class RTPLIVELIBSHARED_EXPORT FECDecodeCache
{
    using List = std::map<uint32_t,PackageVector*>;
public:
    FECDecodeCache();
    
    /**
     * @brief pop
     * 推送包
     * 如果解压条件成功时，将会返回true
     * 否则返回false
     * @param data
     * 数据指针
     * @param len
     * 数据长度
     * @param timestamp
     * 时间戳
     * @param src_nb
     * 原包数量
     * @param repair_nb
     * 冗余包数量
     * @param fill_size
     * 填充的字节,最后组包的时候需要去掉
     * @param pos
     * 该数据包所在位置
     * @param payload_type
     * 用于输出时初始化参数使用
     * @param packet
     * RTPPacket指针
     * 该指针保存着data,而且指针所有权将归本对象,外部不需要对该指针进行释放
     * RTPPacket也含有以上所有参数的数据,当不知道该类内部信息时,建议使用
     * 该函数的重载函数
     * @return 
     */
    bool pop(void *data, const uint64_t &len,
			 const uint32_t& timestamp,
			 const uint16_t& src_nb,
			 const uint16_t& repair_nb,
			 const uint16_t& fill_size,
			 const uint16_t& pos,
			 const int & payload_type,
			 RTPPacket * packet) noexcept;
	
	/**
	 * @brief pop
	 * 重载函数,建议不清楚RTPPacket内部信息时使用
	 * 该接口返回值和另一个不一样
	 * 这里返回的是ts，如果还不能成功解压，则返回0，否则返回正确的时间戳
	 * 可以通过take_at接口获取数据
	 * 其他和重载函数一样
	 */
	uint32_t pop(RTPPacket * rtp_packet) noexcept;
	
	/**
	 * @brief get_correct_timestamp
	 * 获取正确的时间戳信息
	 */
	uint32_t get_correct_timestamp(const uint32_t& ts,
								   const uint16_t& pos) noexcept;
	
	/**
	 * @brief take_at
	 * 将该时间戳之前的所有包取出并释放
	 * 然后取出并返回该时间戳的包
	 * 外部需要在必要的时候释放该内存空间以免造成内存泄漏
	 * @param ts
	 * 时间戳
	 * @return 
	 * 如果不存在该数据，则返回nullptr
	 */
	PackageVector * take_at(const uint32_t& ts) noexcept;
	
	/**
	 * @brief push
	 * 获取第一个包
	 * 如果第一个包符合解码的情况下，将会返回PackageVector对象
	 * 否则返回nullptr
	 * @return 
	 */
	PackageVector * push() noexcept;
	
	/**
	 * @brief remove_first
	 * 移除第一个包
	 */
	void remove_first() noexcept;
	
	/**
	 * @brief lock_source
	 * 锁住资源，用于多线程同步
	 */
	void lock_source() noexcept;
	
	/**
	 * @brief unlock_source
	 * 解锁资源
	 */
	void unlock_source() noexcept;
	
	inline RTPSession::PayloadType get_payload_type() noexcept {
		return cur_pt;
	}
protected:
	/**
	 * @brief insert
	 * 实际添加数据的接口
	 */
	inline PackageVector* insert(void *data, const uint64_t &len,
								 const uint32_t& ts,
								 const uint16_t& src_nb,
								 const uint16_t& repair_nb,
								 const uint16_t& fill_size,
								 const uint16_t& pos,
								 RTPPacket * packet) noexcept;
private:
	List list;
	RTPSession::PayloadType cur_pt{RTPSession::PayloadType::RTP_PT_NONE};
	uint32_t min_timestamp{0};
	core::Timer timer;
	int wait_time{10};
	std::mutex mutex;
};

} //namespace fec

} //namespace rtp_network

} //namespace rtplivelib
