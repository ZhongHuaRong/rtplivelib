#pragma once

#include "abstractplayer.h"
#include <mutex>

namespace rtplivelib {

namespace player {

class AudioPlayerPrivateData;

/**
 * @brief The AudioPlayer class
 * 音频播放器
 * 有两种播放方式
 * 1：通过设置set_player_object传入捕捉类和窗口id，然后start_capture就可以播放了
 *   (这种方式是开多线程播放)
 * 2：调用play接口播放(当前线程播放)
 * 第一种方式播放虽然方便，但是不允许数据前处理，需要继承捕捉类实现on_frame_data实现
 * 
 * 由于play也是异步播放了，所以建议采用第二种方式播放音频
 */
class RTPLIVELIBSHARED_EXPORT AudioPlayer:public AbstractPlayer
{
public:
	AudioPlayer();
	
	virtual ~AudioPlayer() override;
	
	/**
	 * @brief play
	 * 播放音频
	 * 播放操作是异步的，所以该接口会立即返回
	 * 要注意，pakcet包数据在外部最好不要进行改动
	 */
	virtual bool play(core::FramePacket::SharedPacket packet) noexcept override;
private:
	AudioPlayerPrivateData * const d_ptr;
};

} // namespace player

}// namespace rtplivelib
