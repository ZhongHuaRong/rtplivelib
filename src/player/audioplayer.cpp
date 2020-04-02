#include "audioplayer.h"
#include "../core/abstractqueue.h"
#include "../core/logger.h"
#include "SDL2/SDL.h"

namespace rtplivelib {

namespace player {

class AudioPlayerPrivateData{
public:
    //队列，用于获取音频数据
    core::AbstractQueue<core::FramePacket>    audio_data_queue;
    //音频设备
    SDL_AudioSpec wanted_spec;
    //预留，暂时不起什么作用
    core::Format ofmt;
    //用于标识设备是否打开
    bool open_flag{false};
    //每次播放的音频块
    uint8_t  *audio_chunk{nullptr};
    //用于标识音频播放的位置
    uint8_t *audio_pos{nullptr};
    //每块音频剩下的播放长度
    uint32_t  audio_len{0};
    //保存临时的一块数据,防止智能指针释放空间
    core::FramePacket::SharedPacket tmp;
    
    /**
     * @brief open_device
     * 打开设备，
     * @param format
     * 音频格式
     * @param size
     * 一个包的大小，用于计算样本数
     * @return 
     */
    bool open_device(const core::Format &format,int size) noexcept{
        //只通过flag标识来判断是否打开,
        //如果open_flag为false，则一定会尝试打开设备
        if( open_flag == true)
            return true;
        
        constexpr char api[] = "rtplivelib::player::AudioPlayer::open_device";
        wanted_spec.freq = format.sample_rate; 
        switch(format.bits){
        case 8:
            wanted_spec.format = AUDIO_S8; 
            break;
        case 16:
            wanted_spec.format = AUDIO_S16SYS; 
            break;
        case 32:
            wanted_spec.format = AUDIO_F32; 
            break;
        case 64:
        default:
            break;
        }
        wanted_spec.channels = static_cast<uint8_t>(format.channels); 
        wanted_spec.silence = 0; 
        if( format.bits == 0 || format.channels == 0)
            wanted_spec.samples = 512;
        else
            wanted_spec.samples = size / ( format.bits / 8) / format.channels;
        wanted_spec.callback = AudioPlayerPrivateData::fill_audio; 
        wanted_spec.userdata = this;

        if (SDL_OpenAudio(&wanted_spec, nullptr)<0){
            core::Logger::Print_APP_Info(core::Result::SDL_device_open_failed,
                                         api,
                                         LogLevel::ERROR_LEVEL);
            return false; 
        } 
        ofmt = format;
        open_flag = true;
        
        SDL_PauseAudio(0);
        return true;
    }
    
    /**
     * @brief close_device
     * 关闭设备
     */
    void close_device() noexcept{
        if(open_flag == false)
            return;
        //这里不关闭的话，音频输入格式更改的时候就会导致SDL_OpenAudio失败
        SDL_PauseAudio(1);
        SDL_CloseAudio();
        open_flag = false;
    }
    
    /**
     * @brief fill_audio
     * 填充音频数据
     * @param udata
     * 用户数据
     * @param stream
     * @param len
     */
    static 
    void  fill_audio(void * udata,Uint8 *stream,int len){
        if( udata == nullptr)
            return;
        auto ptr = static_cast<AudioPlayerPrivateData*>(udata);

        SDL_memset(stream, INT_MIN, len);
        if(ptr->audio_len == 0){
            ptr->tmp = ptr->audio_data_queue.get_next();
            if( ptr->tmp == nullptr)
                return;
            
            ptr->audio_chunk = ptr->tmp->data[0];
            ptr->audio_pos = ptr->audio_chunk;
            ptr->audio_len = ptr->tmp->size;
//            return; 
        }
        len=(len>ptr->audio_len?ptr->audio_len:len);
     
        SDL_MixAudio(stream,ptr->audio_pos,len,SDL_MIX_MAXVOLUME);
        ptr->audio_pos += len; 
        ptr->audio_len -= len;
    }
};

////////////////////////////////////////////////////////////

AudioPlayer::AudioPlayer():
    AbstractPlayer(PlayFormat::PF_AUDIO),
    d_ptr(new AudioPlayerPrivateData)
{
}

AudioPlayer::~AudioPlayer()
{
    set_player_object(nullptr);
    d_ptr->close_device();
    delete d_ptr;
}

bool AudioPlayer::play(core::FramePacket::SharedPacket packet) noexcept
{
    //格式不一致则重新打开
    if( d_ptr->ofmt != packet->format){
        d_ptr->close_device();
        if(d_ptr->open_device(packet->format,packet->size) == false)
            return false;
    }
    
    d_ptr->audio_data_queue.push_one(packet);
    return true;
}

} // namespace player

}// namespace rtplivelib
