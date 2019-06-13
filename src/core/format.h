#pragma once

#include "config.h"
#include <stdint.h>
#include <string>
#include <memory>

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
	
	/*指保存数据的格式*/
	int pixel_format{-1};
	
	bool operator == (const Format& format){
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
	
	bool operator != (const Format & format){
		return !this->operator ==(format);
	}
	
	const std::string to_string() {
		constexpr char str[] = "format:[ width:%d,height:%d,sample rate:%d,channels:%d,"
							   "bits:%d,format:%d ]";
		char ch[256];
		sprintf(ch,str,width,height,sample_rate,channels,bits,pixel_format);
		return std::string(ch);
	}
};

/**
 * @brief The FramePacket struct
 * 该结构体是本lib运用最频繁的结构体
 * 所有的有关媒体相关的操作都会用到该结构体
 */
struct RTPLIVELIBSHARED_EXPORT FramePacket{
	using SharedPacket = std::shared_ptr<FramePacket>;
	/*保存数据的指针,rgb和yuyv422只需要用到0*/
	/*这里可以通过判断第二位之后的指针，来知道数据结构是不是Ｐ*/
	uint8_t * data[4]{nullptr,nullptr,nullptr,nullptr};
	/*行大小,有时候会因为要数据对齐，一般大于等于width*/
	int linesize[4]{0,0,0,0};
	/*数据长度,只有在data用到第一位的时候有效,如果data用到了多位，则为0*/
	int size{0};
	/*压缩格式,也可以说是rtp协议的有效负载类型pt,设置该字段是为了解码方便,未压缩的数据将会是0*/
	int payload_type{0};
	/*显示时间戳*/
	int64_t pts{0};
	/*解压缩时间戳*/
	int64_t dts{0};
	/*数据格式*/
	Format format;
	/*用于分包，组包，表示该包是第几个分包*/
	int pos{0};
	/*这个指针请不要使用,内部代码使用*/
	void *packet{nullptr};
	/*这个指针请不要使用,内部代码使用*/
	void *frame{nullptr};
	
	FramePacket() = default;
	FramePacket(const FramePacket&) = delete;
	
	//析构函数，默认调用reset_pointer，所以不需要手动调用reset_pointer
	~FramePacket();
	
	/**
	 * @brief copy
	 * 同Copy
	 */
	FramePacket * copy(FramePacket * src);
	
	/**
	 * @brief reset_pointer
	 * 擦除该包所有指针
	 * 所有指针指向nullptr，并且释放空间
	 * 不会造成内存泄露
	 * 和指针无关的参数将不会重置
	 */
	void reset_pointer();
	
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
	
	/**
	 * @brief Copy
	 * 深度拷贝
	 * 将会拷贝src的数据到dst，同时分配空间
	 * @param dst
	 * 目标包，不能为空
	 * 数据指针可以不为空，该函数会自动释放该部分空间
	 * frame和packet参数将不会复制
	 * @param src
	 * 源包，不能为空
	 * @return 
	 * 返回dst
	 */
	static FramePacket * Copy(FramePacket * dst,FramePacket * src);
	
	/**
	 * @brief Make_packet
	 * 先不允许栈上分配,调用该函数获取对象实例
	 * 析构的话，可以调用delete
	 * 也可以调用下面那个函数
	 * @return 
	 */
	static inline FramePacket * Make_Packet(){
		return new FramePacket;
	}
	
	/**
	 * @brief Make_Shared
	 * 构造一个智能指针版对象
	 */
	static inline SharedPacket Make_Shared(){
		return std::make_shared<FramePacket>();
	}
	
	static inline void Destroy_Packet(FramePacket ** packet){
		if(packet == nullptr || (*packet) == nullptr)
			return;
		delete *packet;
		*packet = nullptr;
	}
private:
	FramePacket & operator = (const FramePacket&) = default;
};

} // namespace core
} // namespace rtplivelib

