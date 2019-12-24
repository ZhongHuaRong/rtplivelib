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
	
	SwsContext *scale_ctx{nullptr};
    core::Format ifmt;
    core::Format ofmt;
    std::recursive_mutex mutex;
    
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
    bool scale(uint8_t * src_data[],int src_linesize[],
               uint8_t * dst_data[],int dst_linesize[]) noexcept {
        constexpr char api[] = "device_manager::VPFPrivateData::scale";
        
        const AVPixelFormat & src_fmt = static_cast<AVPixelFormat>(ifmt.pixel_format);
        const AVPixelFormat & dst_fmt = static_cast<AVPixelFormat>(ofmt.pixel_format);
        
        if( src_fmt == AV_PIX_FMT_NONE || dst_fmt == AV_PIX_FMT_NONE )
            return false;
        
        std::lock_guard<std::recursive_mutex> lk(mutex);
        if( scale_ctx == nullptr){
//            sws_freeContext(scale_ctx);
            scale_ctx = sws_getContext(ifmt.width,ifmt.height, src_fmt,
                                       ofmt.width, ofmt.height, dst_fmt,
                                       SWS_BICUBIC,
                                       nullptr, nullptr, nullptr);
            
            /*可能存在上下文分配失败的情况*/
            if( scale_ctx == nullptr){
                core::Logger::Print_APP_Info(core::MessageNum::SwsContext_init_failed,
                                             api,
                                             LogLevel::WARNING_LEVEL);
                return false;
            }
        }
        
        //直接使用src_data和src_linesize，可能会存在数据未对齐的情况，将会影响速度
        //但是内存拷贝也存在开销，还不知道哪个影响比较大，所以先不进行对齐
        auto ret = sws_scale(scale_ctx,src_data,src_linesize,0,ifmt.height,
                        dst_data,dst_linesize);

        if(ret < 0){
            core::Logger::Print_FFMPEG_Info(ret,
											api,
											LogLevel::WARNING_LEVEL);
            return false;
        }
        
        return true;
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

bool Scale::scale(core::FramePacket *dst, core::FramePacket *src) noexcept
{
    if( dst == nullptr || src == nullptr)
        return false;
    //只判断第一个数组
    if( dst->data[0] == nullptr ){
        dst->reset_pointer();
        if(av_image_alloc(dst->data,dst->linesize,d_ptr->ofmt.width,d_ptr->ofmt.height,
                       static_cast<AVPixelFormat>(d_ptr->ofmt.pixel_format),0) < 0 )
            return false;
    }
    return scale(src->data,src->linesize,
                 dst->data,dst->linesize);
}

bool Scale::scale(core::FramePacket::SharedPacket dst, core::FramePacket::SharedPacket src) noexcept
{
    if( dst == nullptr || src == nullptr)
        return false;
    
    return scale(dst.get(),src.get());
}

bool Scale::scale(uint8_t *src_data[], int src_linesize[], uint8_t *dst_data[], int dst_linesize[]) noexcept
{
    if( src_data == nullptr || dst_data == nullptr)
        return false;
    return d_ptr->scale(src_data,src_linesize,dst_data,dst_linesize);
}

} // namespace image_processing

} // namespace rtplivelib
