
#pragma once

#include "../core/abstractqueue.h"
#include "../core/format.h"
#include "../rtp_network/rtpsession.h"
#include "../player/abstractplayer.h"
#include <memory>

namespace rtplivelib {

namespace codec {

class AudioDecoderPrivateData;

/**
 * @brief The AudioDecoder class
 * 音频解码类
 */
class RTPLIVELIBSHARED_EXPORT AudioDecoder :
        public core::AbstractQueue<std::pair<rtp_network::RTPSession::PayloadType,std::shared_ptr<core::FramePacket>>>
{
public:
    using Packet = std::pair<rtp_network::RTPSession::PayloadType,std::shared_ptr<core::FramePacket>>;
    using Queue = core::AbstractQueue<Packet>;
public:
    AudioDecoder();

    virtual ~AudioDecoder() override;

    /**
     * @brief set_player_object
     * 临时接口，用于设置播放器
     */
    void set_player_object(display::AbstractPlayer * player) noexcept;
protected:
    /**
     * @brief on_thread_run
     * 线程运行时需要处理的操作
     */
    virtual void on_thread_run() noexcept override;

    /**
     * @brief get_thread_pause_condition
     * 该函数用于判断线程是否需要暂停
     */
    virtual bool get_thread_pause_condition() noexcept override;
private:
    AudioDecoderPrivateData * const d_ptr;
};

inline bool AudioDecoder::get_thread_pause_condition() noexcept							{		return false;}

}// namespace codec

}// namespace rtplivelib
