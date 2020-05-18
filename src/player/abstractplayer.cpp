#include "abstractplayer.h"
#include "../core/logger.h"
#include <SDL2/SDL.h>
#undef main

namespace rtplivelib{

namespace player {

std::shared_ptr<PlayerEvent> PlayerEvent::EventObject;

void PlayerEvent::deal_event(PlayerEvent *obj) noexcept
{
	SDL_Event event;
	while(obj->thread_flag){
		SDL_WaitEventTimeout(&event,delay);
		switch(event.type){
			case SDL_QUIT:
				return;
			case SDL_WINDOWEVENT:
			{
				switch(event.window.event){
					case ::SDL_WINDOWEVENT_RESIZED:
					case ::SDL_WINDOWEVENT_SIZE_CHANGED:
						if(obj->play_flag){
							obj->play_flag = false;
						}
						obj->lock_time = core::Time::Now();
						break;
					default:
						if(!obj->play_flag){
							if((core::Time::Now() - obj->lock_time).to_timestamp() >= delay){
								obj->play_flag = true;
							}
						}
				}
			}
		}
	}
}

PlayerEvent::PlayerEvent()
{
	thread_flag = true;
	event_thread = new (std::nothrow)std::thread(PlayerEvent::deal_event,this);
	if(!event_thread)
		core::Logger::Print_APP_Info(core::Result::Thread_Create_Failed,
									 __PRETTY_FUNCTION__,
									 LogLevel::WARNING_LEVEL,
									 "PlayerEvent");
	else
		core::Logger::Print_APP_Info(core::Result::Thread_Create_Success,
									 __PRETTY_FUNCTION__,
									 LogLevel::INFO_LEVEL,
									 "PlayerEvent");
}

PlayerEvent::~PlayerEvent()
{
	if(event_thread == nullptr)
		return;
	thread_flag = false;
	SDL_Event user_event;
	user_event.type = SDL_QUIT;
	user_event.user.code=0;
	user_event.user.data1=NULL;
	user_event.user.data2=NULL;
	SDL_PushEvent(&user_event);
	//这里会造成本线程调用该接口造成死锁
	if(event_thread->joinable())
		event_thread->join();
	delete event_thread;
	event_thread = nullptr;
}


///////////////////////////////////////////////////////////////////////////////////

AbstractPlayer::AbstractPlayer(PlayFormat format):
	_play_object(nullptr),
	_fmt(format)
{
	switch (_fmt) {
	case PlayFormat::PF_AUDIO:
		_init_result = SDL_InitSubSystem(SDL_INIT_AUDIO);
		break;
	case PlayFormat::PF_VIDEO:
		_init_result = SDL_InitSubSystem(SDL_INIT_VIDEO);
		break;
	}
	if(_init_result != 0)
		core::Logger::Print(SDL_GetError(),
							__PRETTY_FUNCTION__,
							LogLevel::ERROR_LEVEL);
	if(PlayerEvent::EventObject == nullptr){
		auto p = std::make_shared<PlayerEvent>();
		PlayerEvent::EventObject.swap(p);
	}
}

AbstractPlayer::~AbstractPlayer()
{
	switch (_fmt) {
	case PlayFormat::PF_AUDIO:
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		break;
	case PlayFormat::PF_VIDEO:
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		break;
	}
	
	if(!SDL_WasInit(0)){
		SDL_Quit();
	}
}

void AbstractPlayer::set_player_object(core::AbstractQueue<core::FramePacket> *object,
									   void *winId) noexcept
{
	if(object == nullptr && _play_object == nullptr)
		return;
	auto __object = _play_object;
	_object_mutex.lock();
	_play_object = object;
	_object_mutex.unlock();
	//如果线程正在等待资源
	//先设置好捕捉对象，然后让原有对象离开等待
	//线程的下一个循环就是新的对象了
	if(__object != nullptr)
		__object->exit_wait_resource();
	//新的对象如果是空，则不开线程
	if(_play_object == nullptr){
		exit_thread();
		return;
	}
	
	set_win_id(winId);
	start_thread();
}

void AbstractPlayer::on_thread_run() noexcept
{
	/* 这里的每个步骤都需要很谨慎，因为_play_object随时都会被主线程设置为nullptr
	 * (可能是因为要退出线程，也可能是不需要显示窗口而传入nullptr参数)
	 * 所以在每个需要用到_play_object指针的地方都需要知道是否为空指针*/
	if(_play_object == nullptr)
		return;
	//这个线程暂停条件默认返回false，也就是不会主动暂停
	//一般线程是暂停在这里，等待设备数据的到来
	//一般只需要调用捕捉类的stop_capture即可以暂停线程
	//退出线程也很简单，只需要在set_player_object传入nullptr
	//会默认唤醒线程，然后退出(这个时候_play_object已经是nullptr)
	_play_object->wait_resource_push();
	//这里上锁，感觉问题不大,不用每次取包都上一次锁，
	//上锁的概率太低(只有更换队列的时候才更换)
	std::lock_guard<std::mutex> lk(_object_mutex);
	while(_play_object != nullptr){
		auto pack = _play_object->get_next();
		if(pack == nullptr || pack->data == nullptr)
			break;
		this->play(pack);
	}
}

}// namespace player

} // namespace rtplivelib
