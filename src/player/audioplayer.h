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
	 */
	virtual bool play(core::FramePacket::SharedPacket packet) noexcept override;
	
	/**
	 * @brief show
	 * 显示,可以调用该接口显示图片或者视频
	 * 不过单独使用该接口显示图像时，需要提前设置窗口id和格式
	 * set_win_id和set_video_format
	 * 使用了set_player_object接口显示的时候不要调用该函数
	 * @param format
	 * 图像格式
	 * @param data
	 * 原始图像数据
	 * 需要注意的是多通道图像和单通道图像的区别，建议传入4维数组，未使用的通道设置为nullptr
	 * 用于区分单通道和多通道
	 * @param linesize
	 * 行宽
	 * 注意点同上
	 */
	virtual bool play(const core::Format& format,uint8_t * data[],int linesize[])  noexcept override;
private:
	AudioPlayerPrivateData * const d_ptr;
};

inline bool AudioPlayer::play(core::FramePacket::SharedPacket packet)	noexcept	{
	return AbstractPlayer::play(packet);
}

} // namespace player

}// namespace rtplivelib
