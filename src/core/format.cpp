
#include "format.h"
extern "C"{
#include "libavcodec/avcodec.h"
}

namespace rtplivelib {

namespace core {

FramePacket::~FramePacket(){
	reset_pointer();
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
	memset(packet->data,0,sizeof(packet->data));
	
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
    if( this->data[0] == nullptr )
        return false;
    if(is_packet()){
        return flag & AV_PKT_FLAG_KEY;
    } else {
        return flag;
    }
}

FramePacket *FramePacket::copy(FramePacket *src)
{
    return FramePacket::Copy(this,src);
}

FramePacket &FramePacket::copy(FramePacket &&src)
{
    return FramePacket::Copy(*this,std::forward<FramePacket>(src));
}

FramePacket &FramePacket::copy(FramePacket::SharedPacket &src)
{
    return *FramePacket::Copy(this,src.get());
}

FramePacket * FramePacket::Copy(FramePacket * dst,FramePacket * src){
	if(dst == nullptr || src == nullptr)
		return nullptr;
	dst->reset_pointer();
	*dst = *src;
    //考虑是否使用ffmpeg内置的内存计数
    //现在暂时不用
	dst->frame = nullptr;
	dst->packet = nullptr;
	
    for(auto i = 0;i < 4;++i){
        if(src->data[i] != nullptr){
            dst->data[i] = static_cast<uint8_t *>(av_malloc(static_cast<size_t>(dst->size)));
            if(dst->data[i] != nullptr)
                memcpy(dst->data[i],src->data[i],static_cast<size_t>(dst->size));
        }
    }
	
    return dst;
}

FramePacket &FramePacket::Copy(FramePacket &dst, FramePacket &src)
{
    FramePacket::Copy(&dst,&src);
    return dst;
}

FramePacket &FramePacket::Copy(FramePacket &dst, FramePacket &&src)
{
    //直接浅拷贝
    dst = src;
    memset(src.data,0,sizeof(src.data));
    src.frame = nullptr;
    src.packet = nullptr;
    return dst;
}

FramePacket::SharedPacket &FramePacket::Copy(FramePacket::SharedPacket &dst, FramePacket::SharedPacket &src)
{
    if(src == nullptr)
        return dst;
    if(dst == nullptr){
        SharedPacket && p =  FramePacket::Make_Shared();
        dst.swap(p);
    }
    FramePacket::Copy(dst.get(),src.get());
    return dst;
}

} // namespace core

} // namespace core

