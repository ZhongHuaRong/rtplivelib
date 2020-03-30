
#include "format.h"
#include "logger.h"
extern "C"{
#include "libavcodec/avcodec.h"
//#include "libavutil/mem.h"
}

namespace rtplivelib {

namespace core {

FramePacket::~FramePacket(){
//	(*packet->num) -= 1;
//	if( *packet->num > 0)
//		return;
	reset_pointer();
//	delete packet->num;
}

FramePacket *FramePacket::copy(FramePacket *src)
{
	return FramePacket::Copy(this,src);
}

void FramePacket::reset_pointer()
{
	auto packet = this;
	
	/**
	 * 分两种情况，一种是含有ffmpeg的包，也就是packet指针不为nullptr
	 * 一种是不使用ffmpeg的API采集的数据包，也就是packet指针为空
	 * 这个时候需要自己delete data
	 */
	if(packet->packet == nullptr && packet->frame == nullptr){
		for( auto n = 0; n < 4; ++n){
			if(packet->data[n] != nullptr){
				av_free(packet->data[n]);
			}
		}
	}
	if(packet->packet != nullptr){
		auto ptr = static_cast<AVPacket*>(packet->packet);
		if(ptr->buf != nullptr){
			av_packet_unref(ptr);
		}
		av_packet_free(&ptr);
		packet->packet = nullptr;
	}
	if(packet->frame != nullptr){
		auto ptr = static_cast<AVFrame*>(packet->frame);
		av_frame_unref(ptr);
		av_frame_free(&ptr);
		packet->frame = nullptr;
	}
	memset(packet->data,0,sizeof(uint8_t*) * 4);
	
}

bool FramePacket::is_packet() noexcept
{
	//如果连第一行都没有数据，那肯定是空的
	if( this->data[0] == nullptr )
		return false;
	//packet只会存在第一个数组
	//这里需要区分一下只用到一个数组的frame
	else if(this->linesize[0] != 0)
		return false;
	else
		return true;
}

bool FramePacket::is_frame() noexcept
{
	//如果连第一行都没有数据，那肯定是空的
	if( this->data[0] == nullptr )
		return false;
	else if( this->data[1] != nullptr)
		return true;
	else if(this->linesize[0] != 0)
		return true;
	else
        return false;
}

bool FramePacket::is_key() noexcept
{
    
}

FramePacket * FramePacket::Copy(FramePacket * dst,FramePacket * src){
	if(dst == nullptr || src == nullptr)
		return nullptr;
	dst->reset_pointer();
	*dst = *src;
	dst->frame = nullptr;
	dst->packet = nullptr;
	
	//目前只用到第一行
	if(src->data[0] != nullptr){
		dst->data[0] = static_cast<uint8_t *>(av_malloc(static_cast<size_t>(dst->size)));
		if(dst->data[0] != nullptr)
			memcpy(dst->data[0],src->data[0],static_cast<size_t>(dst->size));
	}
	
	return dst;
}

} // namespace core

} // namespace core

