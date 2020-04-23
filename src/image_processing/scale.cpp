#include "scale.h"
#include "../core/logger.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/dict.h>

#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

namespace rtplivelib{

namespace image_processing{

struct ScalePrivateData{
	
	SwsContext				*scale_ctx{nullptr};
	core::Format			ifmt;
	core::Format			ofmt;
	std::recursive_mutex	mutex;
	
	void release_ctx() noexcept {
		std::lock_guard<std::recursive_mutex> lk(mutex);
		if(scale_ctx != nullptr){
			sws_freeContext(scale_ctx);
			scale_ctx = nullptr;
		}
	}
	
	/**
	 * @brief scale
	 * 数据格式转换,同时也能缩放
	 */
	core::Result scale(uint8_t * src_data[],int src_linesize[],
					   uint8_t * dst_data[],int dst_linesize[]) noexcept {
		
		const AVPixelFormat & src_fmt = static_cast<AVPixelFormat>(ifmt.pixel_format);
		const AVPixelFormat & dst_fmt = static_cast<AVPixelFormat>(ofmt.pixel_format);
		
		if( src_fmt == AV_PIX_FMT_NONE || dst_fmt == AV_PIX_FMT_NONE )
			return core::Result::Format_Error;
		
		std::lock_guard<std::recursive_mutex> lk(mutex);
		if( scale_ctx == nullptr){
			//            sws_freeContext(scale_ctx);
			scale_ctx = sws_getContext(ifmt.width,ifmt.height, src_fmt,
									   ofmt.width, ofmt.height, dst_fmt,
									   SWS_BICUBIC,
									   nullptr, nullptr, nullptr);
			
			/*可能存在上下文分配失败的情况*/
			if( scale_ctx == nullptr){
				return core::Result::SwsContext_init_failed;
			}
		}
		
		//直接使用src_data和src_linesize，可能会存在数据未对齐的情况，将会影响速度
		//但是内存拷贝也存在开销，还不知道哪个影响比较大，所以先不进行对齐
		auto ret = sws_scale(scale_ctx,src_data,src_linesize,0,ifmt.height,
							 dst_data,dst_linesize);
		
		if(ret < 0){
			core::Logger::Print_FFMPEG_Info(ret,
											__PRETTY_FUNCTION__,
											LogLevel::WARNING_LEVEL);
			return core::Result::Scale_Failed;
		}
		
		return core::Result::Success;
	}
};

//////////////////////////////////////////////////

Scale::Scale():
	d_ptr(new ScalePrivateData)
{
	
}

Scale::~Scale()
{
	d_ptr->release_ctx();
	delete d_ptr;
}

void Scale::set_default_output_format(const core::Format &format) noexcept
{
	if( d_ptr->ofmt == format )
		return;
	
	std::lock_guard<std::recursive_mutex> lk(d_ptr->mutex);
	d_ptr->ofmt = format;
	d_ptr->release_ctx();
}

void Scale::set_default_output_format(const int &width, const int &height, const int &pix_fmt) noexcept
{
	if( d_ptr->ofmt.width == width && d_ptr->ofmt.height == height && d_ptr->ofmt.pixel_format == pix_fmt)
		return;
	
	std::lock_guard<std::recursive_mutex> lk(d_ptr->mutex);
	d_ptr->ofmt.width = width;
	d_ptr->ofmt.height = height;
	d_ptr->ofmt.pixel_format = pix_fmt;
	d_ptr->release_ctx();
}

void Scale::set_default_input_format(const core::Format &format) noexcept
{
	if( d_ptr->ifmt == format )
		return;
	
	std::lock_guard<std::recursive_mutex> lk(d_ptr->mutex);
	d_ptr->ifmt = format;
	d_ptr->release_ctx();
}

void Scale::set_default_input_format(const int &width, const int &height, const int &pix_fmt) noexcept
{
	if( d_ptr->ifmt.width == width && d_ptr->ifmt.height == height && d_ptr->ifmt.pixel_format == pix_fmt)
		return;
	
	std::lock_guard<std::recursive_mutex> lk(d_ptr->mutex);
	d_ptr->ifmt.width = width;
	d_ptr->ifmt.height = height;
	d_ptr->ifmt.pixel_format = pix_fmt;
	d_ptr->release_ctx();
}

core::Result Scale::scale(core::FramePacket *dst, core::FramePacket *src) noexcept
{
	if( dst == nullptr || src == nullptr || src->data == nullptr)
		return core::Result::Invalid_Parameter;
	
	if(dst->data == nullptr){
		dst->data = core::DataBuffer::Make_Shared();
		if(dst->data == nullptr)
			return core::Result::Invalid_Parameter;
	}
	uint8_t * data[4]{nullptr,nullptr,nullptr,nullptr};
	int linesize[4]{0,0,0,0};
	
	if(av_image_alloc(data,linesize,d_ptr->ofmt.width,d_ptr->ofmt.height,
					  static_cast<AVPixelFormat>(d_ptr->ofmt.pixel_format),0) < 0 )
		return core::Result::FramePacket_data_alloc_failed;
	
	auto ret = scale(&(*src->data)[0],src->data->linesize,
			data,linesize);
	
	dst->data->set_data(&(data[0]),0);
	for(auto i = 0;i < 4;++i){
		dst->data->linesize[i] = linesize[i];
	}
	
	return ret;
}

core::Result Scale::scale(core::FramePacket::SharedPacket &dst, core::FramePacket::SharedPacket &src) noexcept
{
	if( src == nullptr )
		return core::Result::Invalid_Parameter;
	if( dst == nullptr ){
		core::FramePacket::SharedPacket && p = core::FramePacket::Make_Shared();
		dst.swap(p);
	}
	
	return scale(dst.get(),src.get());
}

core::Result Scale::scale(uint8_t *src_data[], int src_linesize[], uint8_t *dst_data[], int dst_linesize[]) noexcept
{
	if( src_data == nullptr || dst_data == nullptr)
		return core::Result::Invalid_Parameter;
	return d_ptr->scale(src_data,src_linesize,dst_data,dst_linesize);
}

} // namespace image_processing

} // namespace rtplivelib
