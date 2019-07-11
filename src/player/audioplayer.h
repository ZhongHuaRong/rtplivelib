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
 */
class RTPLIVELIBSHARED_EXPORT AudioPlayer:public AbstractPlayer
{
public:
	AudioPlayer();
	
	virtual ~AudioPlayer() override;
	
	/**
	 * @brief show
	 * 重载函数
	 * 该接口不会复制一份
	 */
	virtual bool play(core::FramePacket::SharedPacket packet) noexcept override;
private:
	AudioPlayerPrivateData * const d_ptr;
};

} // namespace player

}// namespace rtplivelib
