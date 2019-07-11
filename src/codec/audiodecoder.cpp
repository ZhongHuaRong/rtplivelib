#include "audiodecoder.h"
#include "../core/logger.h"
extern "C"{
#include "libavcodec/avcodec.h"
}

namespace rtplivelib {

namespace codec {


class AudioDecoderPrivateData{
public:
	using PayloadType = codec::AudioDecoder::Packet::first_type;
	using Packet = codec::AudioDecoder::Packet::second_type;
public:
	AVCodec* decoder{nullptr};
	AVCodecContext * decoder_ctx{nullptr};
	AVCodecParserContext *parser_ctx{nullptr};
	PayloadType cur_pt{PayloadType::RTP_PT_NONE};
	core::Format cur_fmt;
	player::AbstractPlayer *player{nullptr};
	AVPacket * pkt{nullptr};
	AVFrame * frame{nullptr};
	AVFrame * sw_frame{nullptr};
	
	
	AudioDecoderPrivateData(){
	}
	
	~AudioDecoderPrivateData(){
		close_parser_ctx();
		close_codec_ctx();
		av_packet_free(&pkt);
		av_frame_free(&frame);
	}
	
	/**
	 * @brief close_ctx
	 * 关闭解码器的上下文
	 */
	inline void close_codec_ctx() noexcept {
		if(decoder_ctx != nullptr){
			if(pkt != nullptr){
				//清空缓存
				pkt->data = nullptr;
				pkt->size = 0;
				decode(false);
			}
			avcodec_free_context(&decoder_ctx);
		}
	}
	
	/**
	 * @brief close_parser_ctx
	 * 关闭解析器上下文
	 */
	inline void close_parser_ctx() noexcept {
		if( parser_ctx != nullptr){
			av_parser_close(parser_ctx);
			parser_ctx = nullptr;
		}
	}
	
	/**
	 * @brief init_parser_ctx
	 * 初始化解析器上下文
	 * 如果已经初始化，则关闭之前的上下文重新初始化
	 */
	inline void init_parser_ctx() noexcept {
		close_parser_ctx();
		
		parser_ctx = av_parser_init(decoder->id);
		
		constexpr char api[] = "codec::VideoDecoderPD::init_parser_ctx";
		
		if(parser_ctx == nullptr){
			core::Logger::Print_APP_Info(core::MessageNum::Codec_decoder_parser_init_failed,
										 api,
										 LogLevel::WARNING_LEVEL);
			return;
		}
	}
	
	/**
	 * @brief init_packet
	 * 初始化AVPacket，如果已经初始化过则不进行任何操作
	 */
	inline void init_packet() noexcept {
		if( pkt != nullptr)
			return;
		
		pkt = av_packet_alloc();
	}
	
	/**
	 * @brief init_frame
	 * 初始化AVFrame，如果已经初始化过则不进行任何操作
	 */
	inline void init_frame() noexcept {
		if( frame != nullptr)
			return;
		
		frame = av_frame_alloc();
		sw_frame = av_frame_alloc();
	}
	
	/**
	 * @brief check_pt
	 * 检查包的有效载荷类型，如果发现更换了，得及时调整编码上下文
	 */
	inline void check_pt(const PayloadType pt,
						 const Packet & pack) noexcept {
		
		//软解的初始化
		//这里需要注意一下，判断hwd_type_cur是为了防止硬解转为软解的时候的误判
		if( pt == cur_pt && parser_ctx != nullptr && decoder_ctx != nullptr )
			return;
		
		cur_pt = pt;
		
		_init_sw_codec_ctx(cur_pt);
	}
	
	/**
	 * @brief decode
	 * 解码操作
	 * 在这里就不判断各种上下文了，直接开始解码
	 */
	inline void decode(bool play = true) noexcept{
		int ret = 0;
		constexpr char api[] = "codec::VideoDecoderPD::decode";
		
		ret = avcodec_send_packet(decoder_ctx,pkt);
		if( ret < 0 ){
			core::Logger::Print_FFMPEG_Info(ret,api,LogLevel::WARNING_LEVEL);
			return;
		}
		
		while( ret >= 0){
			av_frame_unref(frame);
			ret = avcodec_receive_frame(decoder_ctx,frame);
			//到达终点和解码出错都将退出解码
			if( ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
				return;
			else if( ret < 0){
				core::Logger::Print_FFMPEG_Info(ret,api,LogLevel::WARNING_LEVEL);
				return;
			}
			
//			core::Logger::Print("size:{}",api,LogLevel::INFO_LEVEL,sw_frame->format);
		}
	}
	
	/**
	 * @brief parse
	 * 解析数据
	 */
	inline void parse(uint8_t *data,int size,int64_t pts,int64_t pos) noexcept {
		auto ret = 0;
		constexpr char api[] = "codec::VideoDecoderPD::parse";
		if(parser_ctx == nullptr || decoder_ctx == nullptr){
			core::Logger::Print_APP_Info(core::MessageNum::Codec_parser_or_codec_not_init,
										 api,
										 LogLevel::WARNING_LEVEL);
			return;
		}
		
		while(size > 0){
			//解析数据，这里的步骤和说明文档是一样的
			ret = av_parser_parse2(parser_ctx,decoder_ctx,&pkt->data,&pkt->size,
								   data,size,pts,pts,pos);
			if ( ret < 0 ){
				core::Logger::Print_FFMPEG_Info(ret,api,LogLevel::WARNING_LEVEL);
				return;
			}
			
			data += ret;
			size -= ret;
			
			if( pkt->size )
				decode();
		}
	}
	
	inline void deal_with_pack(const codec::AudioDecoder::Packet& pack) noexcept {
		if(pack.second.get() == nullptr)
			return;
		
		//检查格式并初始化解码器
		check_pt(pack.first,pack.second);
		
		//在这里，需要先初始化Packet和Frame，
		//后面的步骤少不了这两个数据结构
		if(pkt == nullptr){
			init_packet();
			if(pkt == nullptr)
				return;
		}
		
		if(frame == nullptr){
			init_frame();
			if(frame == nullptr)
				return;
		}
		
		parse(pack.second->data[0],pack.second->size,pack.second->pts,pack.second->pos);
	}
	
private:
	/**
	 * @brief _init_encoder
	 * 通过解码器名字初始化编码器,同时初始化解码器上下文
	 * @param name
	 * 硬件加速的解码器名字
	 * @param format
	 * 用于初始化解码器参数
	 * @return 
	 */
	inline bool _init_decoder(const char * name) noexcept {
		decoder = avcodec_find_decoder_by_name(name);
		constexpr char api[] = "codec::VEPD::_init_encoder";
		if(decoder == nullptr){
			core::Logger::Print_APP_Info(core::MessageNum::Codec_decoder_not_found,
										 api,
										 LogLevel::WARNING_LEVEL,
										 name);
			return false;
		}
		else {
			close_codec_ctx();
			decoder_ctx = avcodec_alloc_context3(decoder);
			if(decoder_ctx == nullptr){
				core::Logger::Print_APP_Info(core::MessageNum::Codec_codec_context_alloc_failed,
											 api,
											 LogLevel::WARNING_LEVEL);
				return false;
			}
			
			core::Logger::Print_APP_Info(core::MessageNum::Codec_decoder_init_success,
										 api,
										 LogLevel::INFO_LEVEL,
										 decoder->long_name);
			return true;
		}
	}
	
	/**
	 * @brief init_ctx
	 * 初始化软解用的上下文
	 */
	inline void _init_sw_codec_ctx(PayloadType pt) noexcept {
		
		constexpr char api[] = "codec::VideoDecoderPD::_init_sw_codec_ctx";
		AVCodecID id;
		
		switch (pt) {
		case PayloadType::RTP_PT_HEVC:
			id = AV_CODEC_ID_HEVC;
			break;
		case PayloadType::RTP_PT_H264:
			id = AV_CODEC_ID_H264;
			break;
		case PayloadType::RTP_PT_VP9:
			id = AV_CODEC_ID_VP9;
			break;
		case PayloadType::RTP_PT_AAC:
			id = AV_CODEC_ID_AAC;
			break;
		default:
			id = AV_CODEC_ID_NONE;
			break;
		}
		
		decoder = avcodec_find_decoder(id);
		if(decoder == nullptr) {
			core::Logger::Print_APP_Info(core::MessageNum::Codec_decoder_not_found,
										 api,
										 LogLevel::WARNING_LEVEL,
										 id);
			//没找着格式的话，删除之前初始化过的上下文
			close_parser_ctx();
			close_codec_ctx();
		}
		else {
			//找到id后，初始化上下文
			//该接口会自己删除之前分配的上下文，所以不需要特别处理
			init_parser_ctx();
			
			close_codec_ctx();
			decoder_ctx = avcodec_alloc_context3(decoder);
			
			if(decoder_ctx == nullptr){
				core::Logger::Print_APP_Info(core::MessageNum::Codec_codec_context_alloc_failed,
											 api,
											 LogLevel::WARNING_LEVEL);
				return;
			}
			
			auto ret = avcodec_open2(decoder_ctx,decoder,nullptr);
			if(ret != 0){
				core::Logger::Print_APP_Info(core::MessageNum::Codec_codec_open_failed,
											 api,
											 LogLevel::WARNING_LEVEL);
				core::Logger::Print_FFMPEG_Info(ret,api,LogLevel::WARNING_LEVEL);
			}
		}
	}
};

///////////////////////////////////////////////////////////////////////////////

codec::AudioDecoder::AudioDecoder():
    d_ptr(new AudioDecoderPrivateData)
{
    start_thread();
}

codec::AudioDecoder::~AudioDecoder()
{
    exit_thread();
    delete d_ptr;
}

void AudioDecoder::set_player_object(player::AbstractPlayer *player) noexcept
{
    d_ptr->player = player;
}

void AudioDecoder::on_thread_run() noexcept
{
    //等待资源到来
    //100ms检查一次
    this->wait_for_resource_push(100);

    while(has_data()){
        auto pack = get_next();
        if(pack == nullptr)
            continue;
        d_ptr->deal_with_pack(*pack);
        delete pack;
    }
}

} // namespace codec

} // namespace rtplivelib
