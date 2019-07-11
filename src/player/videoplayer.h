#pragma once

#include "abstractplayer.h"
#include <mutex>

namespace rtplivelib {

namespace player {

class VideoPlayerPrivateData;

/**
 * @brief The VideoPlayer class
 * 视频播放器
 * 有两种播放方式
 * 1：通过设置set_player_object传入捕捉类和窗口id，然后start_capture就可以播放了
 *   (这种方式是开多线程播放)
 * 2：通过set_win_id设置窗口id，然后调用play接口播放(当前线程播放)
 * 第一种方式播放虽然方便，但是不允许数据前处理，需要继承捕捉类实现on_frame_data实现
 */
class RTPLIVELIBSHARED_EXPORT VideoPlayer:public AbstractPlayer
{
public:
	VideoPlayer();
	
	virtual ~VideoPlayer() override;
	
	/**
	 * @brief show_screen_size_changed
	 * 用于显示画面的窗口大小改变时需要调用此函数
	 * 用于重新计算画面
	 * 目前在Windows系统可以正常检测到screen的大小改变
	 * 而Linux系统不能正常检测到，所以设置此函数
	 * Linux需要主动调用此函数,其他系统是内部实现了
	 */
	void show_screen_size_changed(const int & win_w,const int & win_h,
								  const int & frame_w,const int & frame_h);
	
	/**
	 * @brief set_win_id
	 * 设置窗口id
	 * @param id
	 * 窗口id
	 * @param format
	 * 显示的图像格式
	 */
	virtual void set_win_id(void *id)  noexcept override;
	
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
	/**
	 * @brief _set_horizontal_black_border
	 * 设置左右黑边
	 */
	void _set_horizontal_black_border(const int & win_w,const int & win_h,
									  const int & frame_w,const int & frame_h);
	/**
	 * @brief _set_vertical_black_border
	 * 设置上下黑边
	 */
	void _set_vertical_black_border(const int & win_w,const int & win_h,
									const int & frame_w,const int & frame_h);
private:
	VideoPlayerPrivateData * const d_ptr;
};

inline bool VideoPlayer::play(core::FramePacket::SharedPacket packet)	noexcept	{
	return AbstractPlayer::play(packet);
}

} // namespace player

}// namespace rtplivelib
