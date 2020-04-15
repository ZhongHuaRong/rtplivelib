#include "crop.h"

#include "../core/logger.h"
extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
	#include <libavutil/dict.h>
	#include <libavdevice/avdevice.h>

	#include <libavfilter/buffersink.h>
	#include <libavfilter/buffersrc.h>
	#include <libavutil/imgutils.h>
}

namespace rtplivelib{

namespace image_processing{

struct CropPrivateData{
	AVFilterContext *buffersink_ctx{nullptr};
	AVFilterContext *buffersrc_ctx{nullptr};
	AVFilterGraph *filter_graph{nullptr};
	AVFrame * crop_frame_in{nullptr};
	Rect crect;
    core::Format ifmt;
    int ofmt{-1};
    std::recursive_mutex mutex;
	
	/**
	 * @brief init_filter
	 * 初始化滤镜
	 * 需要输入原格式，输出格式是默认YUYV
	 * @param filters_descr
	 * 特效
	 * @return 
	 * 是否成功,小于0是失败
	 */
	inline int init_filter(const char *filters_descr)
	{
		char args[512];
		int ret = 0;
		const AVFilter *buffersrc  = avfilter_get_by_name("buffer");
		const AVFilter *buffersink = avfilter_get_by_name("buffersink");
		AVFilterInOut *outputs = avfilter_inout_alloc();
		AVFilterInOut *inputs  = avfilter_inout_alloc();
		AVPixelFormat opixel = static_cast<AVPixelFormat>(ofmt == -1 ? ifmt.pixel_format:ofmt);
		enum AVPixelFormat pix_fmts[] = { opixel, AV_PIX_FMT_NONE };
		
		std::lock_guard<std::recursive_mutex> lk(mutex);
		filter_graph = avfilter_graph_alloc();
		if (!outputs || !inputs || !filter_graph) {
			ret = AVERROR(ENOMEM);
			goto end;
		}
	
		snprintf(args, sizeof(args),
				"video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
				ifmt.width, ifmt.height, ifmt.pixel_format,
				1, 15,
				1, 1);
	
		ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
										   args, nullptr, filter_graph);
		if (ret < 0) {
			goto end;
		}
	
		/* buffer video sink: to terminate the filter chain. */
		ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
										   nullptr, nullptr, filter_graph);
		if (ret < 0) {
			goto end;
		}
	
		ret = av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts,
								  AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
		if (ret < 0) {
			goto end;
		}
	
		/*
		 * Set the endpoints for the filter graph. The filter_graph will
		 * be linked to the graph described by filters_descr.
		 */
	
		/*
		 * The buffer source output must be connected to the input pad of
		 * the first filter described by filters_descr; since the first
		 * filter input label is not specified, it is set to "in" by
		 * default.
		 */
		outputs->name       = av_strdup("in");
		outputs->filter_ctx = buffersrc_ctx;
		outputs->pad_idx    = 0;
		outputs->next       = nullptr;
	
		/*
		 * The buffer sink input must be connected to the output pad of
		 * the last filter described by filters_descr; since the last
		 * filter output label is not specified, it is set to "out" by
		 * default.
		 */
		inputs->name       = av_strdup("out");
		inputs->filter_ctx = buffersink_ctx;
		inputs->pad_idx    = 0;
		inputs->next       = nullptr;
	
		if ((ret = avfilter_graph_parse_ptr(filter_graph, filters_descr,
										&inputs, &outputs, nullptr)) < 0)
			goto end;
	
		if ((ret = avfilter_graph_config(filter_graph, nullptr)) < 0)
			goto end;
	
	end:
		avfilter_inout_free(&inputs);
		avfilter_inout_free(&outputs);
		if(ret < 0){
			core::Logger::Print_FFMPEG_Info(ret,
											__PRETTY_FUNCTION__,
											LogLevel::WARNING_LEVEL);
			release_filter();
		}
		return ret;
	}
	
	/**
	 * @brief release_filter
	 * 释放滤镜相关的结构体
	 */
	inline void release_filter(){
		std::lock_guard<std::recursive_mutex> lk(mutex);
		avfilter_free(buffersrc_ctx);
		avfilter_free(buffersink_ctx);
		avfilter_graph_free(&filter_graph);
		
		buffersrc_ctx = nullptr;
		buffersink_ctx = nullptr;
        filter_graph = nullptr;
	}
	
	/**
	 * @brief release_crop_frame
	 * 释放裁剪用的frame
	 */
	inline void release_crop_frame(){
        if(crop_frame_in == nullptr)
            return;
		memset(crop_frame_in->data,0,sizeof(crop_frame_in->data));
		av_frame_free(&crop_frame_in);
	}
    
    /**
	 * @brief crop
	 * 裁剪
	 * @param src
	 * 需要注意，这里不检查包是否为空
	 * @return 
	 * 并不是返回形参的那个
	 */
	core::Result crop(core::FramePacket * dst,core::FramePacket * src){

		if(filter_graph == nullptr){
			char descr[512];
			//初始化裁剪滤镜
			sprintf(descr,"crop=%d:%d:%d:%d",crect.width,
											 crect.height,
											 crect.x,
											 crect.y);
			if(init_filter(descr) < 0){
				return core::Result::Crop_Filter_init_failed;
			}
		}
		
		/*初始化裁剪用帧*/
		if( crop_frame_in == nullptr){
			crop_frame_in = av_frame_alloc();
		}
		if( crop_frame_in == nullptr)
			return core::Result::FramePacket_alloc_failed;
		//设置输入帧参数
		crop_frame_in->width = ifmt.width;
		crop_frame_in->height = ifmt.height;
		crop_frame_in->format = ifmt.pixel_format;
		memcpy(crop_frame_in->data,src->data,sizeof(src->data));
		memcpy(crop_frame_in->linesize,src->linesize,sizeof(src->linesize));
		
		AVFrame * crop_frame_out{nullptr};
		{
			std::lock_guard<std::recursive_mutex> lk(mutex);
			//输入帧
			int ret = av_buffersrc_add_frame_flags(buffersrc_ctx, crop_frame_in, AV_BUFFERSRC_FLAG_PUSH);
			if(ret < 0){
				core::Logger::Print_FFMPEG_Info(ret,
												__PRETTY_FUNCTION__,
												LogLevel::WARNING_LEVEL);
				return core::Result::Crop_Failed;
			}
			
			crop_frame_out = av_frame_alloc();
			if( crop_frame_out == nullptr)
                return core::Result::FramePacket_alloc_failed;
			//获取帧
			ret = av_buffersink_get_frame(buffersink_ctx,crop_frame_out);
			if(ret < 0){
				core::Logger::Print_FFMPEG_Info(ret,
												__PRETTY_FUNCTION__,
												LogLevel::WARNING_LEVEL);
                return core::Result::Crop_Failed;
			}
		}
		
		//先删除原有数据
		dst->reset_pointer();
		//设置数据
		memcpy(dst->data,crop_frame_out->data,sizeof(dst->data));
		//浅拷贝一份数据
		memcpy(dst->linesize,crop_frame_out->linesize,sizeof(dst->linesize));
		//设置其他参数
		dst->format.height = crect.height;
		dst->format.width = crect.width;
		dst->format.frame_rate = src->format.frame_rate;
		if(ofmt == -1)
			dst->format.pixel_format = ifmt.pixel_format;
		else
			dst->format.pixel_format = ofmt;
		dst->format.bits = av_get_bits_per_pixel(av_pix_fmt_desc_get(static_cast<AVPixelFormat>(dst->format.pixel_format)));
		dst->frame = crop_frame_out;
		dst->dts = src->dts;
		dst->pts = src->pts;
		return core::Result::Success;
	}
};

///////////////////////////////////////////////////////////

Crop::Crop():
    d_ptr(new CropPrivateData)
{
    
}

Crop::~Crop()
{
    d_ptr->release_filter();
    d_ptr->release_crop_frame();
    delete d_ptr;
}

void Crop::set_default_input_format(const core::Format &input_format) noexcept
{
    if(d_ptr->ifmt == input_format)
        return;
    d_ptr->ifmt = input_format;
    d_ptr->release_filter();
}

void Crop::set_default_output_pixel(int output_pixel) noexcept
{
    if(d_ptr->ofmt == output_pixel)
        return;
    d_ptr->ofmt = output_pixel;
    d_ptr->release_filter();
}

void Crop::set_default_crop_rect(const Rect &rect) noexcept
{
    if(d_ptr->crect == rect)
        return;
    d_ptr->crect = rect;
    d_ptr->release_filter();
}


core::Result Crop::crop(core::FramePacket *dst, core::FramePacket *src) noexcept
{
    if( dst == nullptr || src == nullptr)
        return core::Result::Invalid_Parameter;
    set_default_input_format(src->format);
    return d_ptr->crop(dst,src);
}

core::Result Crop::crop(core::FramePacket::SharedPacket &dst, core::FramePacket::SharedPacket &src) noexcept
{
    if( src == nullptr )
        return core::Result::Invalid_Parameter;
    if( dst == nullptr ){
        core::FramePacket::SharedPacket && p = core::FramePacket::Make_Shared();
        dst.swap(p);
    }
    
    return crop(dst.get(),src.get());
}

const Rect &Crop::get_crop_rect() noexcept
{
    return d_ptr->crect;
}

} // namespace image_processing

} // namespace rtplivelib
