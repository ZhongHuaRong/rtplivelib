#include "audioencoder.h"
#include "../rtp_network/rtpsession.h"
#include "../core/logger.h"
#include "../audio_processing/resample.h"
extern "C"{
#include "libavutil/opt.h"
#include "libavcodec/avcodec.h"
#include "libavutil/imgutils.h"
}

namespace rtplivelib {

namespace codec {

struct CheckFormat{
public:
	/**
	 * @brief check_sample_fmt
	 * 检查编码器是否支持该编码格式
	 * @param codec
	 * 编码器
	 * @param sample_fmt
	 * 格式
	 */
	bool check_sample_fmt(const AVCodec *codec, enum AVSampleFormat sample_fmt) noexcept 
	{
		const enum AVSampleFormat *p = codec->sample_fmts;
	
		while (*p != AV_SAMPLE_FMT_NONE) {
			if (*p == sample_fmt)
				return true;
			p++;
		}
		return false;
	}
	
	/**
	 * @brief select_sample_rate
	 * 选择最大采样率
	 */
	int select_sample_rate(const AVCodec *codec) noexcept 
	{
		const int *p;
		int best_samplerate = 0;
	
		if (!codec->supported_samplerates)
			return 44100;
	
		p = codec->supported_samplerates;
		while (*p) {
			if (!best_samplerate || abs(44100 - *p) < abs(44100 - best_samplerate))
				best_samplerate = *p;
			p++;
		}
		return best_samplerate;
	}
	
	/**
	 * @brief select_channel_layout
	 * 选择通道数最多的布局
	 */
	uint64_t select_channel_layout(const AVCodec *codec) noexcept 
	{
		if (!codec->channel_layouts)
			return AV_CH_LAYOUT_STEREO;
		
		const uint64_t *p;
		uint64_t best_ch_layout = 0;
		int best_nb_channels   = 0;
	
		p = codec->channel_layouts;
		while (*p) {
			int nb_channels = av_get_channel_layout_nb_channels(*p);
	
			if (nb_channels > best_nb_channels) {
				best_ch_layout    = *p;
				best_nb_channels = nb_channels;
			}
			p++;
		}
		return best_ch_layout;
	}
};

///////////////////////////////////////////////////////////////////////////////

AudioEncoder::AudioEncoder():
	Encoder (false,HardwareDevice::HWDType::None)
{
}

AudioEncoder::AudioEncoder(AudioEncoder::Queue *queue):
	Encoder (queue,false,HardwareDevice::HWDType::None)
{
	start_thread();
}

AudioEncoder::~AudioEncoder()
{
	set_input_queue(nullptr);
	close_encoder();
	exit_thread();
	
	if( encode_frame != nullptr){
		av_frame_free(&encode_frame);
	}
	
	if(resample != nullptr){
		delete resample;
	}
}

void AudioEncoder::encode(core::FramePacket *packet) noexcept
{
	//先初始化编码器上下文
	if(packet != nullptr){
		if( _open_ctx(packet) == false){
			return;
		}
	}
	else if(encoder_ctx == nullptr){
		//参数传入空一般是用在清空剩余帧
		//如果上下文也是空，则不处理
		return;
	}
	
	//这里设置好用于编码的frame
	if( _alloc_encode_frame() == false)
		return;
	
	int ret;
	constexpr char api[] = "codec::AEPD::encode";
	bool free_flag{false};
	uint8_t **dst_data = nullptr;
	
	if( packet == nullptr){
		//当编码结束时传入nullptr
		ret = avcodec_send_frame(encoder_ctx, nullptr);
	} else {
		if( ifmt != default_resample_format){
			//需要重采样
			
			//只有在resample重新生成才需要初始化,一般在open_ctx里面就初始化了
			if( resample == nullptr && _init_resample() == false)
				return ;
			
			if( packet->format.channels == 0 && packet->format.bits == 0){
				core::Logger::Print_APP_Info(core::MessageNum::Format_invalid_format,
											 api,
											 LogLevel::WARNING_LEVEL);
				return;
			}
			
			int nb{0},size{0};
			uint8_t **src_data = static_cast<uint8_t**>(packet->data);
			if( resample->resample(&src_data,packet->size / (packet->format.bits * 4 / packet->format.channels),
								   &dst_data,nb,size) == false){
				//日志在resample里面已经输出，这里就不输出了
				return;
			}
			
//				memcpy(encode_frame->data,dst_data,sizeof(uint8_t**) * 4);
			encode_frame->data[0] = dst_data[0];
			free_flag = true;
			encode_frame->nb_samples = nb;
			
		}
		else {
			//不需要重采样
//				memcpy(encode_frame->data,packet->data,sizeof(packet->data));
			//只用到第一个
			encode_frame->data[0] = packet->data[0];
			encode_frame->nb_samples = packet->size / (packet->format.bits * 4 / packet->format.channels);
		}
		
		/* send the frame for encoding */
		ret = avcodec_send_frame(encoder_ctx, encode_frame);
	}
	
	if(ret < 0){
		core::Logger::Print_FFMPEG_Info(ret,api,LogLevel::WARNING_LEVEL);
		if(free_flag){
			av_free(dst_data[0]);
			av_free(dst_data);
		}
		return;
	}
	
	receive_packet();
	
	if(free_flag){
		av_free(dst_data[0]);
		av_free(dst_data);
	}
}

void AudioEncoder::set_encoder_param(const core::Format &format) noexcept
{
	//这里好像初始化其他格式会导致open codec失败
	//所以先设置好默认的格式
	UNUSED(format)
//		switch (format.bits) {
//		case 8:
//			encoder_ctx->sample_fmt = AV_SAMPLE_FMT_U8;
//			break;
//		case 16:
//			encoder_ctx->sample_fmt = AV_SAMPLE_FMT_S16;
//			break;
//		case 32:
//			encoder_ctx->sample_fmt = AV_SAMPLE_FMT_FLT;
//			break;
//		case 64:
//			encoder_ctx->sample_fmt = AV_SAMPLE_FMT_DBL;
//			break;
//		}
	
	encoder_ctx->sample_fmt = AV_SAMPLE_FMT_S16;
	encoder_ctx->bit_rate = 64000;
	
	/* select other audio parameters supported by the encoder */
	encoder_ctx->sample_rate    = 44100;
	encoder_ctx->channel_layout = AV_CH_LAYOUT_STEREO;
	encoder_ctx->channels       = 2;
//		encoder_ctx->profile = FF_PROFILE_UNKNOWN;
}

void AudioEncoder::receive_packet() noexcept
{
	int ret = 0;
	constexpr char api[] = "codec::AEPD::receive_packet";
	AVPacket * src_packet = nullptr;
	
	//其实只有输入nullptr结束编码的时候循环才起作用
	while(ret >= 0){
		src_packet = av_packet_alloc();
		if(src_packet == nullptr){
			core::Logger::Print_APP_Info(core::MessageNum::FramePacket_frame_alloc_failed,
										 api,
										 LogLevel::WARNING_LEVEL);
			return;
		}
		ret = avcodec_receive_packet(encoder_ctx,src_packet);
		if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
			break;
		}
		else if(ret < 0){
			core::Logger::Print_FFMPEG_Info(ret,api,LogLevel::WARNING_LEVEL);
			break;
		}
		//以下操作是拷贝数据
		auto dst_packet = core::FramePacket::Make_Shared();
		if(dst_packet == nullptr){
			core::Logger::Print_APP_Info(core::MessageNum::FramePacket_frame_alloc_failed,
										 api,
										 LogLevel::WARNING_LEVEL);
			break;
		}
		dst_packet->size = src_packet->size;

		//浅拷贝,减少数据的拷贝次数
		dst_packet->data[0] = src_packet->data;
		dst_packet->packet = src_packet;
		//这里设置格式是为了后面发送包的时候设置各种参数
		//感觉这里不需要赋值,对于编码数据，不需要设置格式,只需要设置编码格式
		//好像音频需要用到
		dst_packet->format = default_resample_format;
		//设置有效负载类型，也就是编码格式
		dst_packet->payload_type = payload_type;
		
		dst_packet->pts = src_packet->pts;
		dst_packet->dts = src_packet->dts;
		core::Logger::Print("audio size:{}",
							api,
							LogLevel::ALLINFO_LEVEL,
							dst_packet->size);
		
		//让退出循环时不要释放掉该packet
		src_packet = nullptr;
		this->push_one(dst_packet);
	}
	
	if(src_packet != nullptr)
		av_packet_free(&src_packet);
}

bool AudioEncoder::_alloc_encode_frame() noexcept
{
	
	if( encode_frame == nullptr){
		encode_frame = av_frame_alloc();
		
		if( encode_frame == nullptr)
			return false;
	}
	
	if( encode_frame != nullptr && reassignment == true){
		encode_frame->nb_samples     = encoder_ctx->frame_size;
		encode_frame->format         = encoder_ctx->sample_fmt;
		encode_frame->channel_layout = encoder_ctx->channel_layout;
		reassignment = false;
		return true;
	}
	return true;
}

bool AudioEncoder::_open_ctx(const core::FramePacket *packet) noexcept
{
	if(encoder_ctx != nullptr && ifmt == packet->format){
		return true;
	} else {
		//先关闭之前的编码器(如果存在的话)
		//可能需要更改格式
		close_encoder();
		if( create_encoder("libfdk_aac") == false){
			return false;
		}
	}
	constexpr char api[] = "codec::AEPD::open_ctx";
	set_encoder_param(packet->format);
	auto ret = avcodec_open2(encoder_ctx,encoder,nullptr);
	if(ret < 0){
		core::Logger::Print_APP_Info(core::MessageNum::Codec_codec_open_failed,
									 api,
									 LogLevel::WARNING_LEVEL);
		core::Logger::Print_FFMPEG_Info(ret,
									 api,
									 LogLevel::WARNING_LEVEL);
		return false;
	}
	ifmt = packet->format;
	reassignment = true;
	payload_type = rtp_network::RTPSession::PayloadType::RTP_PT_AAC;
	
	_init_resample();
	return true;
}

bool AudioEncoder::_init_resample() noexcept
{
	if( resample == nullptr){
		resample = new audio_processing::Resample;
		if( resample == nullptr)
			return false;
	}
	
	resample->set_default_output_format(default_resample_format);
	resample->set_default_input_format(ifmt);
	return true;
}

} // namespace codec

} // namespace rtplivelib
