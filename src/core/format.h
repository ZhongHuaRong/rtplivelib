#pragma once

#include "config.h"
#include <stdint.h>
#include <string>
#include <memory>
#include <mutex>

class AVPacket;
class AVFrame;

namespace rtplivelib {

namespace core {

/**
 * @brief The Format struct
 * 视音频的格式
 * 当视频独占的数值不为０时，则是视频格式，否则是音频
 */
struct Format{
	/**
	 * 视频独占
	 * 长宽
	 */
	int width{0};
	int height{0};
	
	/**
	 * 视频独占
	 * 这个参数不参与格式比较
	 * 只用于记录当前帧数
	 */
	int frame_rate{0};
	
	/**
	 * 音频独占
	 * 采样率和声道数
	 */
	int sample_rate{0};
	int channels{0};
	
	/**
	 * 视频是指一个像素所占位数
	 * 音频指一个采样值的一个声道所占位数
	 */
	int bits{0};
	
	/*指保存数据的格式,这里的格式太多了，先不列出来，内部使用，
		可以查看avutil/pixfmt.h文件查看AVPixelFormat结构*/
	int pixel_format{-1};
	
	bool operator == (const Format& format) noexcept{
		//这里的判断可能不太严谨
		if(this->width == 0 && format.width == 0){
			//应该是音频
			if( this->sample_rate == format.sample_rate &&
					this->bits == format.bits &&
					this->channels == format.channels &&
					this->pixel_format == format.pixel_format)
				return true;
			else
				return false;
		}
		else{
			//应该是视频
			if( this->width == format.width && 
					this->height == format.height &&
					this->bits == format.bits &&
					this->pixel_format == format.pixel_format)
				return true;
			else
				return false;
		}
	}
	
	bool operator != (const Format & format) noexcept{
		return !this->operator ==(format);
	}
	
	const std::string to_string()  noexcept{
		constexpr char str[] = "format:[ width:%d,height:%d,sample rate:%d,channels:%d,"
							   "bits:%d,format:%d ]";
		char ch[256];
		sprintf(ch,str,width,height,sample_rate,channels,bits,pixel_format);
		return std::string(ch);
	}
};

/**
 * @brief The DataBuffer struct
 * 保存数据的结构体，可以像指针一样使用
 * 用智能指针包装了一层，可以用计数共享内存空间
 * 多线程使用[]操作符读写数据时需要使用lock
 * 其他接口不需要调用lock,会死锁
 */
struct RTPLIVELIBSHARED_EXPORT DataBuffer {
	using SharedBuffer = std::shared_ptr<DataBuffer>;
public:
	/*构造函数只能选择赋值其中一个，或者不传参数，
	 *如果传了两个可能出现不可预期的错误*/
	DataBuffer(void * packet = nullptr,
			   void * frame = nullptr);
	
	~DataBuffer();
	
	DataBuffer& operator() (const DataBuffer &) = delete;
	
	/*模拟数组指针,外部可以通过该接口获取数据,如果要进行写操作，先调用lock接口*/
	uint8_t* & operator[] (const int &) noexcept;
	
	inline DataBuffer& set_data(uint8_t ** src,size_t size) noexcept{
		return DataBuffer::SetData(*this,src,size);
	}
	inline DataBuffer& set_data(uint8_t * src,size_t size) noexcept{
		return DataBuffer::SetData(*this,src,size);
	}
	inline DataBuffer& set_data_no_lock(uint8_t ** src,size_t size) noexcept{
		return DataBuffer::SetData_NoLock(*this,src,size);
	}
	inline DataBuffer& set_data_no_lock(uint8_t * src,size_t size) noexcept{
		return DataBuffer::SetData_NoLock(*this,src,size);
	}
	inline DataBuffer& copy_data(uint8_t ** src,size_t size) noexcept{
		return DataBuffer::CopyData(*this,src,size);
	}
	inline DataBuffer& copy_data(uint8_t * src,size_t size) noexcept{
		return DataBuffer::CopyData(*this,src,size);
	}
	inline DataBuffer& copy_data_no_lock(uint8_t ** src,size_t size) noexcept{
		return DataBuffer::CopyData_NoLock(*this,src,size);
	}
	inline DataBuffer& copy_data_no_lock(uint8_t * src,size_t size) noexcept{
		return DataBuffer::CopyData_NoLock(*this,src,size);
	}
	
	DataBuffer& copy_data(DataBuffer &buf) noexcept;
	DataBuffer& copy_data(DataBuffer &&buf) noexcept;
	
	/**
	 * @brief SetData
	 * 从外部浅拷贝数据到类
	 * 使用该接口会减少计数，计数为0则释放之前的空间
	 * @param dst
	 * 拷贝的目标
	 * @param src
	 * 源数据,是一个四维数组
	 * @param size
	 * 对应src的数据长度
	 * @return 
	 * 返回dst
	 */
	static DataBuffer& SetData(DataBuffer& dst,uint8_t ** src,size_t size) noexcept;
	
	/*重载函数，应用于一维数据*/
	static DataBuffer& SetData(DataBuffer& dst,uint8_t * src,size_t size) noexcept;
	
	/*应用于不需要上锁的情景*/
	static DataBuffer& SetData_NoLock(DataBuffer& dst,uint8_t ** src,size_t size) noexcept;
	
	/*重载函数，应用于一维数据*/
	static DataBuffer& SetData_NoLock(DataBuffer& dst,uint8_t * src,size_t size) noexcept;
	/**
	 * @brief CopyData
	 * 从外部深度拷贝数据到类
	 * 使用该接口会减少计数，计数为0则释放之前的空间
	 */
	static DataBuffer& CopyData(DataBuffer& dst,uint8_t ** src,size_t size) noexcept;
	/*重载函数，应用于一维数据*/
	static DataBuffer& CopyData(DataBuffer& dst,uint8_t * src,size_t size) noexcept;
	/*重载函数，应用DataBuffer*/
	static DataBuffer& CopyData(DataBuffer& dst,DataBuffer& src) noexcept;
	static DataBuffer& CopyData(DataBuffer& dst,DataBuffer&& src) noexcept;
	
	/*应用于不需要上锁的情景*/
	static DataBuffer& CopyData_NoLock(DataBuffer& dst,uint8_t ** src,size_t size) noexcept;
	/*重载函数，应用于一维数据*/
	static DataBuffer& CopyData_NoLock(DataBuffer& dst,uint8_t * src,size_t size) noexcept;
	
	
	inline static constexpr int GetDataPtrSize() noexcept{
		return sizeof(DataBuffer().data);
	}
	
	inline static SharedBuffer Make_Shared(void * packet = nullptr,
										   void * frame = nullptr) noexcept{
		return std::make_shared<DataBuffer>(packet,frame);
	}
	
	/**
	 * @brief is_packet
	 * 判断该类存的数据是不是包
	 * 指压缩后或解压前的数据
	 * 如果没有数据则一定是false
	 */
	bool is_packet() noexcept;
	
	/**
	 * @brief is_frame
	 * 判断该类存的数据是不是帧
	 * 指压缩前或者解压后的数据
	 * 如果没有数据则一定是false
	 */
	bool is_frame() noexcept;
	
	/*上锁，保证操作安全*/
	inline void lock() noexcept{
		mutex.lock();
	}
	
	/*解锁*/
	inline void unlock() noexcept{
		mutex.unlock();
	}
	
	/*内部使用*/
	void set_packet(void * packet) noexcept;
	void set_packet_no_lock(void * packet) noexcept;
	
	/*内部使用*/
	void set_frame(void * frame) noexcept;
	void set_frame_no_lock(void * frame) noexcept;
protected:
	/**
	 * @brief clear
	 * 清空所有数据
	 */
	void clear() noexcept;
private:
	/*内部使用*/
	void _set_packet(AVPacket * packet) noexcept;
	
	/*内部使用*/
	void _set_frame(AVFrame * frame) noexcept;
public:
	/*行大小,有时候会因为要数据对齐，一般大于等于width*/
	int						linesize[4]{0,0,0,0};
	/*数据长度,只有在data用到第一位的时候有效,如果data用到了多位，则为0*/
	size_t					size;
	/*数据读写保护锁,不想用接口的可以直接用这个结构体上锁*/
	std::recursive_mutex	mutex;
private:
	/*保存数据的指针,rgb和yuyv422只需要用到0*/
	/*这里可以通过判断第二位之后的指针，来知道数据结构是不是P*/
	uint8_t					*data[4]{nullptr,nullptr,nullptr,nullptr};
	void					*packet{nullptr};
	void					*frame{nullptr};
	
	friend class std::shared_ptr<DataBuffer>;
};

/**
 * @brief The FramePacket struct
 * 该结构体是本lib运用最频繁的结构体
 * 所有的有关媒体相关的操作都会用到该结构体
 * 该类在此项目中基本是同一时间只在一个线程读写，所以不需要上锁
 * 需要注意两层智能指针的关系
 * data应该上锁后读写
 */
struct RTPLIVELIBSHARED_EXPORT FramePacket{
	using SharedPacket = std::shared_ptr<FramePacket>;
	using SharedBuffer = DataBuffer::SharedBuffer;
	/*共享内存的数据空间,可以像指针一样使用该结构
	 * 如果需要写数据则先深度拷贝*/
	SharedBuffer		data;
	/*压缩格式,也可以说是rtp协议的有效负载类型pt,设置该字段是为了解码方便,未压缩的数据将会是0*/
	int					payload_type{0};
	/*显示时间戳*/
	int64_t				pts{0};
	/*解压缩时间戳*/
	int64_t				dts{0};
	/*数据格式*/
	Format				format;
	/*用于分包，组包，表示该包是第几个分包*/
	int					pos{0};
	/*用于表示是否为关键帧，如果需要用到则调用is_key接口*/
	int					flag{0};
	
	FramePacket() = default;
	FramePacket & operator = (const FramePacket&) = default;
	~FramePacket();
	
	/**
	 * @brief is_key
	 * 判断该包是否含有关键帧
	 * @return 
	 */
	bool is_key() noexcept;
	
	/**
	 * @brief Make_packet
	 * 堆上分配对象
	 * @return 
	 */
	attribute_deprecated
	static inline FramePacket * Make_Packet() noexcept{
		auto ptr =  new (std::nothrow)FramePacket;
		if(ptr != nullptr)
			ptr->data = DataBuffer::Make_Shared();
		return ptr;
	}
	
	/**
	 * @brief Make_Shared
	 * 构造一个智能指针版对象
	 */
	static inline SharedPacket Make_Shared() noexcept{
		auto ptr = std::make_shared<FramePacket>();
		if(ptr != nullptr)
			ptr->data = DataBuffer::Make_Shared();
		return ptr;
	}
	
	/**
	 * @brief Destroy_Packet
	 * 删除一个FramePacket对象
	 * 尽量使用智能指针而不是使用裸指针
	 * @param packet
	 */
	attribute_deprecated
	static inline void Destroy_Packet(FramePacket ** packet) noexcept{
		if(packet == nullptr || (*packet) == nullptr)
			return;
		delete *packet;
		*packet = nullptr;
	}
};

} // namespace core
} // namespace rtplivelib

