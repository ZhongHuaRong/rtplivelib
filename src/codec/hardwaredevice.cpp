
#include "hardwaredevice.h"
#include "../core/logger.h"
extern "C"{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/hwcontext.h"
#include "libavutil/hwcontext_qsv.h"
#include "libavutil/buffer.h"
#include "libavutil/mem.h"
}

namespace rtplivelib {

namespace codec {

class HardwareDevicePrivateData{
public:
    HardwareDevicePrivateData(){
        
    }
    
    ~HardwareDevicePrivateData(){
        close_hwdevice_ctx();
    }
    
    inline void close_hwdevice_ctx() noexcept {
        if(hw_device_ctx != nullptr){
			av_buffer_unref(&hw_device_ctx);
            hw_device_ctx = nullptr;
        }
    }
    
    /**
     * @brief create_hwdevice_ctx
     * 根据硬件设备类型创建上下文，会关闭前一次的设置
     * @param type
     * 硬件设备类型
     * @return 
     */
    inline bool create_hwdevice_ctx(AVHWDeviceType type) noexcept {
        close_hwdevice_ctx();
          
        {
            auto ret = av_hwdevice_ctx_create(&hw_device_ctx, type,
                                             nullptr, nullptr, 0);
            if( ret < 0){
                core::Logger::Print_APP_Info(core::Result::Codec_hardware_ctx_create_failed,
											 __PRETTY_FUNCTION__,
                                             LogLevel::WARNING_LEVEL,
                                             av_hwdevice_get_type_name(type));
                core::Logger::Print_FFMPEG_Info(ret,
												__PRETTY_FUNCTION__,
                                                LogLevel::WARNING_LEVEL);
                hwdevice_type = AV_HWDEVICE_TYPE_NONE;
            }
            else {
                core::Logger::Print_APP_Info(core::Result::Codec_hardware_ctx_create_success,
											 __PRETTY_FUNCTION__,
                                             LogLevel::INFO_LEVEL,
                                             av_hwdevice_get_type_name(type));
                hwdevice_type = type;
            }
            return ret >= 0;
        }
    }
    
    /**
     * @brief set_encoder_ctx
     * 设置编码的上下文,用于初始化编码器
     * @param ctx
     * 编码器的上下文，需要提前初始化好
     * @return 
     */
    inline bool set_encoder_ctx(AVCodecContext * ctx) noexcept
    {
		if( hw_device_ctx == nullptr)
			return false;
		
		AVBufferRef *hw_frames_ref{nullptr};
		AVHWFramesContext *frames_ctx{nullptr};
		
		int err = 0;
	
		if ((hw_frames_ref = av_hwframe_ctx_alloc(hw_device_ctx)) == nullptr) {
			core::Logger::Print_APP_Info(core::Result::Codec_hard_frames_create_failed,
										 __PRETTY_FUNCTION__,
										 LogLevel::WARNING_LEVEL,
										 av_hwdevice_get_type_name(hwdevice_type));
			return false;
		}
		
		void * _data = static_cast<void *>(hw_frames_ref->data);
		frames_ctx = static_cast<AVHWFramesContext *>(_data);
		
		switch(hwdevice_type){
		case AV_HWDEVICE_TYPE_VDPAU:
			ctx->pix_fmt = AV_PIX_FMT_VDPAU;
			frames_ctx->format = AV_PIX_FMT_VDPAU;
            frames_ctx->sw_format = AV_PIX_FMT_NV12;
			break;
		case AV_HWDEVICE_TYPE_CUDA:
			ctx->pix_fmt = AV_PIX_FMT_CUDA;
			frames_ctx->format = AV_PIX_FMT_CUDA;
            frames_ctx->sw_format = AV_PIX_FMT_NV12;
            ctx->gop_size = 250;
            ctx->max_b_frames = 0;
            ctx->bit_rate = 1024 * 1024;
			break;
		case AV_HWDEVICE_TYPE_VAAPI:
			//?
			ctx->pix_fmt = AV_PIX_FMT_VAAPI;
			frames_ctx->format = AV_PIX_FMT_VAAPI;
            frames_ctx->sw_format = AV_PIX_FMT_NV12;
			break;
		case AV_HWDEVICE_TYPE_DXVA2:
			ctx->pix_fmt = AV_PIX_FMT_DXVA2_VLD;
			frames_ctx->format = AV_PIX_FMT_DXVA2_VLD;
			frames_ctx->sw_format = AV_PIX_FMT_NV12;
			break;
		case AV_HWDEVICE_TYPE_QSV:
			ctx->pix_fmt = AV_PIX_FMT_QSV;
			frames_ctx->format = AV_PIX_FMT_QSV;
            frames_ctx->sw_format = AV_PIX_FMT_NV12;
            frames_ctx->initial_pool_size = 20;
            ctx->gop_size = 10;
            ctx->max_b_frames = 1;
            ctx->bit_rate = 12000;
			break;
		case AV_HWDEVICE_TYPE_VIDEOTOOLBOX:
			ctx->pix_fmt = AV_PIX_FMT_VIDEOTOOLBOX;
			frames_ctx->format = AV_PIX_FMT_VIDEOTOOLBOX;
            frames_ctx->sw_format = AV_PIX_FMT_NV12;
			break;
		case AV_HWDEVICE_TYPE_D3D11VA:
			ctx->pix_fmt = AV_PIX_FMT_D3D11;
			frames_ctx->format = AV_PIX_FMT_D3D11;
            frames_ctx->sw_format = AV_PIX_FMT_NV12;
			break;
		case AV_HWDEVICE_TYPE_DRM:
			ctx->pix_fmt = AV_PIX_FMT_DRM_PRIME;
			frames_ctx->format = AV_PIX_FMT_DRM_PRIME;
            frames_ctx->sw_format = AV_PIX_FMT_NV12;
			break;
		case AV_HWDEVICE_TYPE_OPENCL:
			ctx->pix_fmt = AV_PIX_FMT_OPENCL;
			frames_ctx->format = AV_PIX_FMT_OPENCL;
            frames_ctx->sw_format = AV_PIX_FMT_NV12;
			break;
		case AV_HWDEVICE_TYPE_MEDIACODEC:
			ctx->pix_fmt = AV_PIX_FMT_MEDIACODEC;
			frames_ctx->format = AV_PIX_FMT_MEDIACODEC;
            frames_ctx->sw_format = AV_PIX_FMT_NV12;
			break;
		default:
			av_buffer_unref(&hw_frames_ref);
			return false;
		}
		
		frames_ctx->width     = ctx->width;
		frames_ctx->height    = ctx->height;
		
		if ((err = av_hwframe_ctx_init(hw_frames_ref)) < 0) {
			core::Logger::Print_FFMPEG_Info(err,
											__PRETTY_FUNCTION__,
											LogLevel::WARNING_LEVEL);
			av_buffer_unref(&hw_frames_ref);
			return false;
		}
		
		ctx->hw_frames_ctx = av_buffer_ref(hw_frames_ref);
		if (!ctx->hw_frames_ctx)
			core::Logger::Print_APP_Info(core::Result::Codec_set_hard_frames_ctx_failed,
										 __PRETTY_FUNCTION__,
										 LogLevel::WARNING_LEVEL);
	
		av_buffer_unref(&hw_frames_ref);
		return true;
	}
	
	/**
	 * @brief set_decoder_ctx
	 * 设置解码器上下文
	 * @param ctx
	 * 解码器上下文，需要提前初始化好
	 * @return 
	 */
	inline bool set_decoder_ctx(AVCodecContext * ctx) noexcept{
		ctx->opaque = hw_device_ctx;
		
		ctx->get_format = [](struct AVCodecContext *avctx, const enum AVPixelFormat *pix_fmts) -> enum AVPixelFormat{
			while (*pix_fmts != AV_PIX_FMT_NONE) {
				if (*pix_fmts == AV_PIX_FMT_QSV) {
					AVBufferRef *hw_device_ref = static_cast<AVBufferRef*>(avctx->opaque);
					AVHWFramesContext  *frames_ctx;
					AVQSVFramesContext *frames_hwctx;
					int ret;
		
					/* create a pool of surfaces to be used by the decoder */
					avctx->hw_frames_ctx = av_hwframe_ctx_alloc(hw_device_ref);
					if (!avctx->hw_frames_ctx)
						return AV_PIX_FMT_NONE;
					frames_ctx   = (AVHWFramesContext*)avctx->hw_frames_ctx->data;
					frames_hwctx = static_cast<AVQSVFramesContext*>(frames_ctx->hwctx);
		
					frames_ctx->format            = AV_PIX_FMT_QSV;
					frames_ctx->sw_format         = avctx->sw_pix_fmt;
					frames_ctx->width             = FFALIGN(avctx->coded_width,  32);
					frames_ctx->height            = FFALIGN(avctx->coded_height, 32);
					frames_ctx->initial_pool_size = 32;
		
					frames_hwctx->frame_type = MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET;
		
					ret = av_hwframe_ctx_init(avctx->hw_frames_ctx);
					if (ret < 0){
						core::Logger::Print_FFMPEG_Info(ret,
														__PRETTY_FUNCTION__,
														LogLevel::WARNING_LEVEL);
						return AV_PIX_FMT_NONE;
					}
		
					return AV_PIX_FMT_QSV;
				}
		
				pix_fmts++;
			}
			return AV_PIX_FMT_NONE;
		};
		
		return true;
	}
	
	/**
	 * @brief set_decoder_ctx2
	 * 设置解码器的上下文,版本2
	 * @param ctx
	 * 解码器上下文
	 * @param codec
	 * 解码器
	 * @return 
	 */
	inline bool set_decoder_ctx2(AVCodecContext * ctx,AVCodec * codec) noexcept {
		for (auto i = 0;; i++) {
			const AVCodecHWConfig *config = avcodec_get_hw_config(codec, i);
			if (!config) {
				core::Logger::Print("Decoder {} does not support device type {}.",
									__PRETTY_FUNCTION__,
									LogLevel::WARNING_LEVEL,
									codec->name, av_hwdevice_get_type_name(hwdevice_type));
				return false;
			}
			if ( (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) &&
				config->device_type == hwdevice_type) {
				hw_pix_fmt = config->pix_fmt;
				break;
			}
		}
		
		ctx->opaque = &hw_pix_fmt;
		ctx->get_format = [](AVCodecContext *ctx,const enum AVPixelFormat *pix_fmts) ->enum AVPixelFormat {
			const enum AVPixelFormat *p;
			AVPixelFormat fmt = *static_cast<AVPixelFormat*>(ctx->opaque);
	
			for (p = pix_fmts; *p != AV_PIX_FMT_NONE; p++) {
				if (*p == fmt)
					return *p;
			}
			
			core::Logger::Print("Failed to get HW surface format..",
								__PRETTY_FUNCTION__,
								LogLevel::DEBUG_LEVEL);
			return AV_PIX_FMT_NONE;
		};
        return true;
	}
private:
    AVBufferRef *hw_device_ctx{nullptr};
    AVHWDeviceType hwdevice_type{AV_HWDEVICE_TYPE_NONE};
    AVPixelFormat hw_pix_fmt{AV_PIX_FMT_NONE};
};

HardwareDevice::HardwareDevice():
    d_ptr(new HardwareDevicePrivateData()),
    _init_result(false)
{
}

HardwareDevice::~HardwareDevice()
{
    delete d_ptr;
}

int HardwareDevice::get_hwframe_pix_fmt() noexcept
{
    switch(_type){
    case QSV:
        return AV_PIX_FMT_QSV;
    case NVIDIA:
        //?
        return AV_PIX_FMT_CUDA;
    default:
        return AV_PIX_FMT_NV12;
    }
}

int HardwareDevice::get_swframe_pix_fmt() noexcept
{
    switch(_type){
    //这两个默认返回NV12
    case QSV:
    case NVIDIA:
        return AV_PIX_FMT_NV12;
    default:
        return AV_PIX_FMT_NV12;
    }
}

bool HardwareDevice::init_device(void *codec_ctx,void * codec,HardwareDevice::HWDType type) noexcept
{
    if(codec_ctx == nullptr || codec == nullptr)
        return false;
    AVCodecContext * ctx = static_cast<AVCodecContext*>(codec_ctx);
    AVCodec * _c = static_cast<AVCodec*>(codec);
    
    bool ret = false;
    switch(type){
    case HardwareDevice::HWDType::QSV:
        ret = d_ptr->create_hwdevice_ctx(AV_HWDEVICE_TYPE_QSV);
        break;
    case HardwareDevice::HWDType::DXVA:
        ret = d_ptr->create_hwdevice_ctx(AV_HWDEVICE_TYPE_DXVA2);
        break;
    case HardwareDevice::HWDType::NVIDIA:
        ret = d_ptr->create_hwdevice_ctx(AV_HWDEVICE_TYPE_CUDA);
        break;
    default:
        return false;
    }
    if(ret == false)
        return false;
        
    if( av_codec_is_encoder(_c) ){
        //编码方案
        if( d_ptr->set_encoder_ctx(ctx) == false)
            return false;
    }
    else {
        //解码方案
        //这里说明一下,qsv好像不支持set_decoder_ctx2方式的初始化
        //所以在set_decoder_ctx2返回false的时候，尝试set_decoder_ctx
        //如果还是不行在软解
        if( d_ptr->set_decoder_ctx2(ctx,_c) == false){
            if( d_ptr->set_decoder_ctx(ctx) == false)
                return false;
        }
    }
    
    auto result = avcodec_open2(ctx,_c,nullptr);
    _init_result = (result >= 0);
    if(!_init_result)
        core::Logger::Print_FFMPEG_Info(result,
										__PRETTY_FUNCTION__,
                                        LogLevel::WARNING_LEVEL);
    return _init_result;
}

bool HardwareDevice::close_device(void *codec_ctx) noexcept
{
    if(codec_ctx == nullptr)
        return false;
    AVCodecContext * ctx = static_cast<AVCodecContext*>(codec_ctx);
    avcodec_free_context(&ctx);
    d_ptr->close_hwdevice_ctx();
    return true;
}

} // namespace codec

} // namespace rtplivelib
