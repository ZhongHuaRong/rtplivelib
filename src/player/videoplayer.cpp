#include "videoplayer.h"
#include "../core/logger.h"
extern "C" {
#include "SDL2/SDL.h"
#include "SDL2/SDL_vulkan.h"
#include "libavutil/pixfmt.h"
}

namespace rtplivelib {

namespace player {

class VideoPlayerPrivateData{
public:
	rtplivelib::core::Format show_format;
	void * show_id{nullptr};
	SDL_Window *show_screen{nullptr};
	SDL_Renderer* renderer{nullptr};
	SDL_Texture* texture{nullptr};
	std::mutex show_mutex;
	SDL_Rect *show_rect{new SDL_Rect()};
	int frame_w{0};
	int frame_h{0};
	
	VideoPlayerPrivateData(){}
	
	inline void release_window(){
		release_renderer();
		if(show_screen != nullptr){
			SDL_DestroyWindow(show_screen);
			show_screen = nullptr;
		}
	}
	
	inline void release_renderer(){
		release_texture();
		if(renderer != nullptr){
			SDL_DestroyRenderer(renderer);
			renderer = nullptr;
		}
	}
	
	inline void release_texture(){
		if(texture != nullptr){
			SDL_DestroyTexture(texture);
			texture = nullptr;
		}
	}
	
	/**
	 * @brief create_window
	 * 这里不负责判断window是否已存在
	 * 需要自行使用release_window
	 * @param id
	 */
	inline void create_window(void *id){
		show_id = id;
		if(id == nullptr)
			return;
		show_screen = SDL_CreateWindowFrom(show_id);
		if(show_screen == nullptr){
			constexpr char api[] = "rtplivelib::display::VideoPlayer::create_window";
			core::Logger::Print_APP_Info(core::MessageNum::SDL_window_create_failed,
										 api,
										 LogLevel::WARNING_LEVEL);
			core::Logger::Print(SDL_GetError(),
								api,
								LogLevel::WARNING_LEVEL);
		}
		else
			SDL_ShowWindow(show_screen);
		create_renderer();
	}
	
	/**
	 * @brief create_renderer
	 * 创建渲染器
	 */
	inline void create_renderer(){
		//硬件加速有问题，先设置成软件渲染
		renderer = SDL_CreateRenderer(show_screen, -1, SDL_RENDERER_SOFTWARE);
		if(renderer == nullptr){
			constexpr char api[] = "rtplivelib::display::VideoPlayer::create_renderer";
			core::Logger::Print_APP_Info(core::MessageNum::SDL_renderer_create_failed,
										 api,
										 LogLevel::WARNING_LEVEL);
			core::Logger::Print(SDL_GetError(),
								api,
								LogLevel::WARNING_LEVEL);
		}
	}
	
	/**
	 * @brief set_format
	 * 在show之前，检查格式，然后分配合适的纹理
	 * @param format
	 */
	inline void set_format(const rtplivelib::core::Format& format){
		if(show_format == format)
			return;
		release_texture();
		if(renderer == nullptr)
			create_renderer();
		
		auto & input_fmt = format;
		switch(input_fmt.pixel_format){
		//由于编解码是要带P格式类型的
		//而采集的时候是不带P的，所以需要灵活转换
		//这里的话P和不带P的都是同一种
		case AV_PIX_FMT_YUYV422:
		case AV_PIX_FMT_YUV422P:
			texture = SDL_CreateTexture(renderer,SDL_PIXELFORMAT_YUY2, SDL_TEXTUREACCESS_STREAMING,
										input_fmt.width,input_fmt.height);
			break;
		case AV_PIX_FMT_YUV420P:
			texture = SDL_CreateTexture(renderer,SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,
										input_fmt.width,input_fmt.height);
			break;
		case AV_PIX_FMT_QSV:
		case AV_PIX_FMT_NV12:
			texture = SDL_CreateTexture(renderer,SDL_PIXELFORMAT_NV12, SDL_TEXTUREACCESS_STREAMING,
										input_fmt.width,input_fmt.height);
			break;
		case AV_PIX_FMT_BGRA:
			texture = SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
										input_fmt.width,input_fmt.height);
			break;
		default:
			break;
		}
		
		constexpr char api[] = "rtplivelib::display::VideoPlayer::set_format";
		if(texture == nullptr){
			core::Logger::Print_APP_Info(core::MessageNum::SDL_texture_create_failed,
										 api,
										 LogLevel::WARNING_LEVEL);
			core::Logger::Print(SDL_GetError(),
								api,
								LogLevel::WARNING_LEVEL);
		}
		
		show_format = format;
		core::Logger::Print_APP_Info(core::MessageNum::SDL_show_format_update,
									 api,
									 LogLevel::INFO_LEVEL,
									 show_format.to_string().c_str());
	}
};

VideoPlayer::VideoPlayer():
	AbstractPlayer (PlayFormat::PF_VIDEO),
	d_ptr(new VideoPlayerPrivateData)
{
	d_ptr->show_rect->x = 0;
	d_ptr->show_rect->y = 0;
	d_ptr->show_rect->w = 0;
	d_ptr->show_rect->h = 0;
}

VideoPlayer::~VideoPlayer()
{
	set_player_object(nullptr);
	d_ptr->release_window();
	
	delete d_ptr->show_rect;
	delete d_ptr;
}

/**
 * @brief show_screen_size_changed
 * 用于显示画面的窗口大小改变时需要调用此函数
 * 用于重新计算画面
 * 目前在Windows系统可以正常检测到screen的大小改变
 * 而Linux系统不能正常检测到，所以设置此函数
 * Linux需要主动调用此函数,其他系统是内部实现了
 * @param width
 * 宽
 * @param height
 * 高
 */
void VideoPlayer::show_screen_size_changed(const int &win_w,const int & win_h,
										   const int & frame_w,const int & frame_h)
{
	if(win_w == (d_ptr->show_rect->x * 2 + d_ptr->show_rect->w) &&
			win_h == (d_ptr->show_rect->y * 2 + d_ptr->show_rect->h) &&
			d_ptr->frame_w == frame_w && d_ptr->frame_h == frame_h)
		return;
	//高比宽大
	if (win_h > win_w) {
		//图像宽比高长
		if(frame_w > frame_h){
			_set_vertical_black_border(win_w,win_h,frame_w,frame_h);
		}
		//图片高比宽长
		else{
			//原图高一点
			if (static_cast<double>(frame_w) / static_cast<double>(frame_h) >
					static_cast<double>(win_h) / static_cast<double>(win_w))
				_set_horizontal_black_border(win_w,win_h,frame_w,frame_h);
			else
				_set_vertical_black_border(win_w,win_h,frame_w,frame_h);
		}
	}
	//宽大
	else {
		//图片高比宽长
		if (win_h > win_w){
			_set_horizontal_black_border(win_w,win_h,frame_w,frame_h);
		}
		//图片宽比高长
		else{
			if (static_cast<double>(frame_w) / static_cast<double>(frame_h) >
					static_cast<double>(win_w) / static_cast<double>(win_h))
				//原图宽一点
				_set_vertical_black_border(win_w,win_h,frame_w,frame_h);
			else
				_set_horizontal_black_border(win_w,win_h,frame_w,frame_h);
		}
	}
#if defined (unix)
	//Linux系统的需要重新设置窗口大小
	SDL_SetWindowSize(d_ptr->show_screen,d_ptr->show_rect->w,d_ptr->show_rect->h);
#endif
}

/**
 * @brief set_win_id
 * 更改窗口id
 */
void VideoPlayer::set_win_id(void *id) noexcept
{
	//如果和之前的设置差不多，不处理
	if(id == d_ptr->show_id)
		return;
	std::lock_guard<std::mutex> lk(d_ptr->show_mutex);
	//先释放原来的
	d_ptr->release_window();
	//分配新的窗口，如果为空则不处理
	d_ptr->create_window(id);
}

/**
 * @brief show
 * 显示
 */
bool VideoPlayer::play(const core::Format& format,uint8_t * data[],int linesize[]) noexcept
{
	//如果显示窗口不存在或者图片不存在，则不显示，其他情况都可以
	if(d_ptr->show_screen == nullptr || data ==nullptr)
		return false;
	std::lock_guard<std::mutex> lk(d_ptr->show_mutex);
#ifndef unix
	int h,w;
	SDL_GetWindowSize(d_ptr->show_screen,&w,&h);
	show_screen_size_changed(w,h,format.width,format.height);
#endif
	d_ptr->set_format(format);
	
	if(d_ptr->texture == nullptr || d_ptr->renderer == nullptr)
		return false;
	
	//这里判断一下单通道和多通道
	switch(format.pixel_format){
	case AV_PIX_FMT_YUYV422:
	case AV_PIX_FMT_YUV422P:
	case AV_PIX_FMT_BGRA:
		if( data[0] == nullptr)
			return false;
		SDL_UpdateTexture( d_ptr->texture, nullptr, data[0], linesize[0]); 
		break;
	case AV_PIX_FMT_YUV420P:
		if( data[0] == nullptr || data[1] == nullptr || data[2] == nullptr)
			return false;
		SDL_UpdateYUVTexture( d_ptr->texture, nullptr,
							  data[0],linesize[0],
							  data[1],linesize[1],
							  data[2],linesize[2]);
		break;
	case AV_PIX_FMT_QSV:
	case AV_PIX_FMT_NV12:
		if( data[0] == nullptr || data[1] == nullptr)
			return false;
		SDL_UpdateTexture( d_ptr->texture, nullptr, data[0], linesize[0]); 
//		SDL_UpdateYUVTexture( d_ptr->texture, nullptr,
//							  data[0],linesize[0],
//							  data[1],linesize[1],
//							  data[2],linesize[2] );
		break;
	default:
		return false;
	}
	
	SDL_RenderClear( d_ptr->renderer );   
	auto ret = SDL_RenderCopy( d_ptr->renderer, d_ptr->texture, nullptr, d_ptr->show_rect); 
	if(ret != 0){
		core::Logger::Print(SDL_GetError(),
							"rtplivelib::display::VideoPlayer::show",
							LogLevel::WARNING_LEVEL);
		return false;
	}
	SDL_RenderPresent( d_ptr->renderer ); 
	return true;
}

void VideoPlayer::_set_horizontal_black_border(const int & win_w,const int & win_h,
											   const int & frame_w,const int & frame_h)
{
	auto w = static_cast<double>(frame_w) / 
			 static_cast<double>(frame_h) * static_cast<double>(win_h);
    d_ptr->show_rect->x = static_cast<int>((win_w - w) / 2);
	d_ptr->show_rect->y = 0;
	d_ptr->show_rect->w = static_cast<int>(w);
	d_ptr->show_rect->h = win_h;
	d_ptr->frame_h = frame_h;
	d_ptr->frame_w = frame_w;
}

void VideoPlayer::_set_vertical_black_border(const int & win_w,const int & win_h,
											 const int & frame_w,const int & frame_h)
{
	auto h = static_cast<double>(frame_h) / 
			 static_cast<double>(frame_w) * win_w;
	d_ptr->show_rect->x = 0;
	d_ptr->show_rect->y = static_cast<int>((win_h - h) / 2);
	d_ptr->show_rect->w = win_w;
	d_ptr->show_rect->h = static_cast<int>(h);
	d_ptr->frame_h = frame_h;
	d_ptr->frame_w = frame_w;
}

//////////////////////////////////////////////////////////////////////////////////////

AudioPlayer::AudioPlayer():
	AbstractPlayer (PlayFormat::PF_AUDIO)
{
	
}

} // namespace display

}// namespace rtplivelib
