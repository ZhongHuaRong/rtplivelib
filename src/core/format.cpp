
#include "format.h"
extern "C"{
#include "libavcodec/avcodec.h"
}

namespace rtplivelib {

namespace core {

DataBuffer::DataBuffer(void *packet, void *frame)
{
	if(packet != nullptr){
		_set_packet(static_cast<AVPacket*>(packet));
	}
	
	if(frame != nullptr){
		_set_frame(static_cast<AVFrame*>(frame));
	}
}

DataBuffer::~DataBuffer()
{
	clear();
}

uint8_t *&DataBuffer::operator[](const int &n) noexcept
{
	return data[n];
}

DataBuffer &DataBuffer::copy_data(DataBuffer &buf) noexcept
{
	std::lock_guard<decltype (mutex)> lg(mutex);
	std::lock_guard<decltype (mutex)> lg2(buf.mutex);
	clear();
	
	if(buf.packet == nullptr && buf.frame == nullptr){
		for(auto i = 0;i < 4;++i){
			if(buf[i] != nullptr){
				data[i] = static_cast<uint8_t *>(av_malloc(size));
				if(data[i] != nullptr)
					memcpy(data[i],buf[i],size);
			}
		}
	}
	
	if(buf.packet != nullptr){
		AVPacket * dst = av_packet_alloc();
		if(dst == nullptr)
			return *this;
		AVPacket * src = static_cast<AVPacket*>(buf.packet);
		av_packet_ref(dst,src);
		_set_packet(dst);
	}
	
	if(buf.frame != nullptr){
		AVFrame * dst = av_frame_alloc();
		if(dst == nullptr)
			return *this;
		AVFrame * src = static_cast<AVFrame*>(buf.frame);
		av_frame_ref(dst,src);
		_set_frame(dst);
	}
	size = buf.size;
	memcpy(this->linesize,buf.linesize,sizeof(this->linesize));
	return *this;
}

DataBuffer &DataBuffer::copy_data(DataBuffer &&buf) noexcept
{
	std::lock_guard<decltype (mutex)> lg(mutex);
	clear();
	
	memcpy(data,buf.data,sizeof(data));
	memcpy(linesize,buf.linesize,sizeof(linesize));
	packet = buf.packet;
	frame = buf.frame;
	size = buf.size;
	
	memset(buf.data,0,sizeof(data));
	memset(buf.linesize,0,sizeof(linesize));
	buf.packet = nullptr;
	buf.frame = nullptr;
	buf.size = 0;
	
	return *this;
}

DataBuffer &DataBuffer::SetData(DataBuffer &dst, uint8_t **src, size_t size) noexcept
{
	std::lock_guard<decltype (dst.mutex)> lg(dst.mutex);
	return SetData_NoLock(dst,src,size);
}

DataBuffer &DataBuffer::SetData(DataBuffer &dst, uint8_t *src, size_t size) noexcept
{
	std::lock_guard<decltype (dst.mutex)> lg(dst.mutex);
	return SetData_NoLock(dst,src,size);
}

DataBuffer &DataBuffer::SetData_NoLock(DataBuffer &dst, uint8_t **src, size_t size) noexcept
{
	dst.clear();
	
	memcpy(dst.data,src,sizeof(dst.data));
	dst.size = size;
	return dst;
}

DataBuffer &DataBuffer::SetData_NoLock(DataBuffer &dst, uint8_t *src, size_t size) noexcept
{
	dst.clear();
	
	dst.data[0] = src;
	dst.size = size;
	return dst;
}

DataBuffer &DataBuffer::CopyData(DataBuffer &dst, uint8_t **src, size_t size) noexcept
{
	std::lock_guard<decltype (dst.mutex)> lg(dst.mutex);
	return CopyData_NoLock(dst,src,size);
}

DataBuffer &DataBuffer::CopyData(DataBuffer &dst, uint8_t *src, size_t size) noexcept
{
	std::lock_guard<decltype (dst.mutex)> lg(dst.mutex);
	return CopyData_NoLock(dst,src,size);
}

DataBuffer &DataBuffer::CopyData(DataBuffer &dst, DataBuffer &src) noexcept
{
	return dst.copy_data(src);
}

DataBuffer &DataBuffer::CopyData(DataBuffer &dst, DataBuffer &&src) noexcept
{
	return dst.copy_data(std::move(src));
}

DataBuffer &DataBuffer::CopyData_NoLock(DataBuffer &dst, uint8_t **src, size_t size) noexcept
{
	dst.clear();
	
	for(auto i = 0;i < 4;++i){
		if(src[i] != nullptr){
			dst.data[i] = static_cast<uint8_t *>(av_malloc(size));
			if(dst.data[i] != nullptr)
				memcpy(dst.data[i],src[i],size);
		}
	}
	dst.size = size;
	return dst;
}

DataBuffer &DataBuffer::CopyData_NoLock(DataBuffer &dst, uint8_t *src, size_t size) noexcept
{
	dst.clear();
	
	if(src != nullptr){
		dst.data[0] = static_cast<uint8_t *>(av_malloc(size));
		if(dst.data[0] != nullptr)
			memcpy(dst.data[0],src,size);
		else 
			return dst;
	}
	dst.size = size;
	return dst;
}

bool DataBuffer::is_packet() noexcept
{
	//如果连第一行都没有数据，那肯定是空的
	if( data[0] == nullptr )
		return false;
	//packet只会存在第一个数组
	//这里需要区分一下只用到一个数组的frame
	else if(this->linesize[0] != 0)
		return false;
	else
		return true;
}

bool DataBuffer::is_frame() noexcept
{
	//如果连第一行都没有数据，那肯定是空的
	if( data[0] == nullptr )
		return false;
	else if( data[1] != nullptr)
		return true;
	else if(this->linesize[0] != 0)
		return true;
	else
		return false;
}

void DataBuffer::set_packet(void *packet) noexcept
{
	std::lock_guard<decltype (mutex)> lg(mutex);
	set_packet_no_lock(packet);
}

inline void DataBuffer::set_packet_no_lock(void *packet) noexcept
{
	clear();
	if(packet == nullptr)
		return;
	_set_packet(static_cast<AVPacket*>(packet));
}

void DataBuffer::set_frame(void *frame) noexcept
{
	std::lock_guard<decltype (mutex)> lg(mutex);
	set_frame_no_lock(frame);
}

inline void DataBuffer::set_frame_no_lock(void *frame) noexcept
{
	clear();
	if(frame == nullptr)
		return;
	_set_frame(static_cast<AVFrame*>(frame));
}

void DataBuffer::clear() noexcept
{
	/**
	 * 分两种情况，一种是含有ffmpeg的包，也就是packet指针不为nullptr
	 * 一种是不使用ffmpeg的API采集的数据包，也就是packet指针为空
	 * 这个时候需要自己delete data
	 */
	if(this->packet == nullptr && this->frame == nullptr){
		for( auto n = 0; n < 4; ++n){
			if(this->data[n] != nullptr){
				av_free(this->data[n]);
			}
		}
	}
	if(this->packet != nullptr){
		auto ptr = static_cast<AVPacket*>(this->packet);
		if(ptr->buf != nullptr){
			av_packet_unref(ptr);
		}
		av_packet_free(&ptr);
		this->packet = nullptr;
	}
	if(this->frame != nullptr){
		auto ptr = static_cast<AVFrame*>(this->frame);
		av_frame_unref(ptr);
		av_frame_free(&ptr);
		this->frame = nullptr;
	}
	memset(this->data,0,sizeof(this->data));
	memset(this->linesize,0,sizeof(this->linesize));
}

inline void DataBuffer::_set_packet(AVPacket *packet) noexcept
{
	data[0] = packet->data;
	size = packet->size;
	this->packet = packet;
}

inline void DataBuffer::_set_frame(AVFrame *frame) noexcept
{
	memcpy(data,frame->data,sizeof(data));
	memcpy(linesize,frame->linesize,sizeof(linesize));
	this->frame = frame;
}

/////////////////////////////////////////////////////////////////////////////////////////

FramePacket::~FramePacket(){
}

bool FramePacket::is_key() noexcept
{
	if( (*this->data)[0] == nullptr )
		return false;
	if( data->is_packet() ){
		return flag & AV_PKT_FLAG_KEY;
	} else {
		return flag;
	}
}

} // namespace core

} // namespace core

