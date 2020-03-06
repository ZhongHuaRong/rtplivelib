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
    std::recursive_mutex mutex;
    
    void release_ctx() noexcept {
        std::lock_guard<std::recursive_mutex> lk(mutex);
        if(swr_ctx != nullptr){
            swr_free(&swr_ctx);
        }
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
    
    /**
     * @brief resample
     * 重采样的实际操作
     * 该接口不检查参数是否没问题
     * @param src_data
     * 输入的数据
     * @param src_nb_samples
     * 输入的样本数
     * @param dst_data
     * 输出数据，这个参数最好是nullptr，即*dst_data == nullptr
     * @param dst_nb_samples
     * 输出的样本数，这个将会在内部赋值
     * @param buffer_size
     * 返回输出音频所占用的空间大小
     * @return 
     */
    inline bool resample(uint8_t *** src_data,const int &src_nb_samples,
                         uint8_t *** dst_data,int & dst_nb_samples,
                         int & buffer_size) noexcept{
        
        constexpr char api[] = "device_manager::ResamplePrivateData::resample";
        
        std::lock_guard<std::recursive_mutex> lk(mutex);
        if( swr_ctx == nullptr) {
            if(init_ctx() == false)
                return false;
        }
        
        /* 计算输出样本数 */
        dst_nb_samples =
                av_rescale_rnd(swr_get_delay(swr_ctx, ifmt.sample_rate) + src_nb_samples,
                               ofmt.sample_rate,
                               ifmt.sample_rate,
                               AV_ROUND_UP);
        
        int dst_linesize;
        uint8_t **data{nullptr};
        int ret = av_samples_alloc_array_and_samples(&data, &dst_linesize, ofmt.channels,
                                                     dst_nb_samples, get_sample_format(ofmt.bits), 0);
        if( ret < 0){
            core::Logger::Print_APP_Info(core::Result::FramePacket_data_alloc_failed,
                                         api,
                                         LogLevel::WARNING_LEVEL);
            core::Logger::Print_FFMPEG_Info(ret,
                                            api,
                                            LogLevel::WARNING_LEVEL);
            return false;
        }

        ret = swr_convert(swr_ctx, data, dst_nb_samples, (const uint8_t **)(*src_data), src_nb_samples);
        if (ret < 0) {
            core::Logger::Print_FFMPEG_Info(ret,
                                            api,
                                            LogLevel::WARNING_LEVEL);
            return false;
        }
        
        buffer_size = av_samples_get_buffer_size(&dst_linesize, ofmt.channels,
                                                 ret, get_sample_format(ofmt.bits), 1);
        if (buffer_size < 0) {
            core::Logger::Print_FFMPEG_Info(ret,
                                            api,
                                            LogLevel::WARNING_LEVEL);
            return false;
        }
        
        //成功后需要释放原来的数据占用
        if( *dst_data != nullptr){
            av_free(*dst_data);
        }
        *dst_data = data;
        return true;
    }
protected:
    /**
     * @brief init_ctx
     * 初始化重采样上下文
     */
    bool init_ctx() noexcept{
        constexpr char api[] = "device_manager::ResamplePrivateData::init_ctx";
        release_ctx();
        swr_ctx = swr_alloc();
        if(!swr_ctx){
            core::Logger::Print_APP_Info(core::Result::SwrContext_init_failed,
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
    
        //初始化上下文
        int ret;
        if((ret = swr_init(swr_ctx)) < 0){
            core::Logger::Print_APP_Info(core::Result::SwrContext_init_failed,
                                         api,
                                         LogLevel::WARNING_LEVEL);
            core::Logger::Print_FFMPEG_Info(ret,
                                            api,
                                            LogLevel::WARNING_LEVEL);
            return false;
        }
        
        return true;
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
    if( dst == nullptr || src == nullptr)
        return false;
    
    std::lock_guard<std::recursive_mutex> lk(d_ptr->mutex);
    if( src->format != d_ptr->ifmt)
        return false;
    
    auto src_data = static_cast<uint8_t**>(src->data);
    uint8_t ** data{nullptr};
    int nb_samples{0};
    int size{0};
    auto block_size = src->format.bits * 4 / src->format.channels;
    
    auto ret = resample(&src_data, src->size / block_size ,&data ,nb_samples,size);
    
    if(ret == true){
        dst->reset_pointer();
        
        dst->format = d_ptr->ofmt;
        //这里赋值需要注意一下 
        dst->data[0] = data[0];
        av_free(data);
        dst->size = size;
        return true;
    }
    
    return false;
}

bool Resample::resample(core::FramePacket::SharedPacket dst, core::FramePacket::SharedPacket src) noexcept
{
    if( dst == nullptr || src == nullptr)
        return false;
    
    return resample(dst.get(),src.get());
}

bool Resample::resample(uint8_t *** src_data,const int &src_nb_samples,
                        uint8_t *** dst_data,int & dst_nb_samples,
                        int & buffer_size) noexcept
{
    //不允许输入为空且不允许src数据为空
    if(src_data == nullptr || *src_data == nullptr || dst_data == nullptr )
        return false;
    
    if( src_nb_samples == 0)
        return true;
    
    return d_ptr->resample(src_data,src_nb_samples,dst_data,dst_nb_samples,buffer_size);
}


} // namespace audio_processing

} // namespace rtplivelib
