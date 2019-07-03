#include "resample.h"
#include <mutex>
#include "../core/logger.h"
extern "C"{
#include "libavutil/opt.h"
#include "libavutil/channel_layout.h"
#include "libavutil/samplefmt.h"
#include "libswresample/swresample.h"
}

namespace rtplivelib{

namespace audio_processing{

class ResamplePrivateData{
public:
    SwrContext * swr_ctx{nullptr};
    core::Format ifmt;
    core::Format ofmt;
    uint8_t **src_data{nullptr};
    uint8_t **dst_data{nullptr};
    std::recursive_mutex mutex;
    
    void release_ctx() noexcept {
        std::lock_guard<std::recursive_mutex> lk(mutex);
        if(swr_ctx != nullptr){
            swr_free(&swr_ctx);
        }
        
        if(src_data != nullptr){
            av_freep(&(src_data)[0]);
        }
        av_freep(&src_data);
    
        if(dst_data != nullptr){
            av_freep(&(dst_data)[0]);
        }
        av_freep(&dst_data);
    }
    
    inline long long get_channel_layout( const int &channels) noexcept{
        switch (channels) {
        case 0:
            return AV_CH_LAYOUT_MONO;
        case 1:
            return AV_CH_LAYOUT_MONO;
        case 2:
            return AV_CH_LAYOUT_STEREO;
        default:
            return AV_CH_LAYOUT_MONO;
        }
    }
    
    inline enum AVSampleFormat get_sample_format(const int & bits) noexcept{
        switch (bits) {
        case 8:
            return AV_SAMPLE_FMT_U8;
        case 16:
            return AV_SAMPLE_FMT_S16;
        case 32:
            return AV_SAMPLE_FMT_FLT;
        case 64:
            return AV_SAMPLE_FMT_DBL;
        default:
            return AV_SAMPLE_FMT_NONE;
        }
    }
    
    inline bool resample() noexcept{
        
        constexpr char api[] = "device_manager::ResamplePrivateData::resample";
        
        std::lock_guard<std::recursive_mutex> lk(mutex);
        if( swr_ctx == nullptr) {
            if(init_ctx() == false)
                return false;
        }
        
        
    }
protected:
    bool init_ctx() noexcept{
        constexpr char api[] = "device_manager::ResamplePrivateData::init_ctx";
        release_ctx();
        swr_ctx = swr_alloc();
        if(!swr_ctx){
            core::Logger::Print_APP_Info(core::MessageNum::SwrContext_init_failed,
                                         api,
                                         LogLevel::WARNING_LEVEL);
            return false;
        }
        
        av_opt_set_int(swr_ctx, "in_channel_layout",    get_channel_layout(ifmt.channels), 0);
        av_opt_set_int(swr_ctx, "in_sample_rate",       ifmt.sample_rate, 0);
        av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", get_sample_format(ifmt.bits), 0);
    
        av_opt_set_int(swr_ctx, "out_channel_layout",    get_channel_layout(ofmt.channels), 0);
        av_opt_set_int(swr_ctx, "out_sample_rate",       ofmt.sample_rate, 0);
        av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", get_sample_format(ofmt.bits), 0);
    
        int ret;
        if((ret = swr_init(swr_ctx)) < 0){
            core::Logger::Print_APP_Info(core::MessageNum::SwrContext_init_failed,
                                         api,
                                         LogLevel::WARNING_LEVEL);
            core::Logger::Print_FFMPEG_Info(ret,
                                            api,
                                            LogLevel::WARNING_LEVEL);
            return false;
        }
        
        
    }
};

/////////////////////////////////////////////////////////////////////////////

Resample::Resample():
    d_ptr(new ResamplePrivateData)
{
    
}

Resample::~Resample()
{
    d_ptr->release_ctx();
    delete d_ptr;
}

void Resample::set_default_output_format(const core::Format &format) noexcept
{
    if( d_ptr->ofmt == format )
        return;
    
    std::lock_guard<std::recursive_mutex> lk(d_ptr->mutex);
    d_ptr->ofmt = format;
    d_ptr->release_ctx();
}

void Resample::set_default_output_format(const int &sample_rate, const int &channels, const int &bits) noexcept
{
    if( d_ptr->ofmt.sample_rate == sample_rate 
            && d_ptr->ofmt.channels == channels
            && d_ptr->ofmt.bits == bits)
        return;
    
    std::lock_guard<std::recursive_mutex> lk(d_ptr->mutex);
    d_ptr->ofmt.sample_rate = sample_rate;
    d_ptr->ofmt.channels = channels;
    d_ptr->ofmt.bits = bits;
    d_ptr->release_ctx();
}

void Resample::set_default_input_format(const core::Format &format) noexcept
{
    if( d_ptr->ifmt == format )
        return;
    
    std::lock_guard<std::recursive_mutex> lk(d_ptr->mutex);
    d_ptr->ifmt = format;
    d_ptr->release_ctx();
}

void Resample::set_default_input_format(const int &sample_rate, const int &channels, const int &bits) noexcept
{
    if( d_ptr->ifmt.sample_rate == sample_rate
            && d_ptr->ifmt.channels == channels
            && d_ptr->ifmt.bits == bits)
        return;
    
    std::lock_guard<std::recursive_mutex> lk(d_ptr->mutex);
    d_ptr->ifmt.sample_rate = sample_rate;
    d_ptr->ifmt.channels = channels;
    d_ptr->ifmt.bits = bits;
    d_ptr->release_ctx();
}

bool Resample::resample(core::FramePacket *dst, core::FramePacket *src) noexcept
{
    
}

bool Resample::resample(core::FramePacket::SharedPacket dst, core::FramePacket::SharedPacket src) noexcept
{
    
}


} // namespace audio_processing

} // namespace rtplivelib
