#include "audiodecoder.h"
#include "../core/logger.h"
extern "C"{
#include "libavcodec/avcodec.h"
}

namespace rtplivelib {

namespace codec {


class AudioDecoderPrivateData{
public:
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

void AudioDecoder::set_player_object(display::AbstractPlayer *player) noexcept
{
//    d_ptr->player = player;
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
//        d_ptr->deal_with_pack(*pack);
        delete pack;
    }
}

} // namespace codec

} // namespace rtplivelib
