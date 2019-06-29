#include "audioencoder.h"

namespace rtplivelib {

namespace codec {

class AudioEncoderPrivateData{
public:
};

AudioEncoder::AudioEncoder():
	_queue(nullptr),
	d_ptr(new AudioEncoderPrivateData())
{
}

AudioEncoder::AudioEncoder(AudioEncoder::Queue *queue):
	_queue(queue),
	d_ptr(new AudioEncoderPrivateData())
{
	start_thread();
}

AudioEncoder::~AudioEncoder()
{
	set_input_queue(nullptr);
	exit_thread();
	delete d_ptr;
}

int AudioEncoder::get_encoder_id() noexcept
{
//	if(d_ptr->encoder == nullptr)
//		return 0;
//	return d_ptr->encoder->id;
	return 0;
}

void AudioEncoder::on_thread_run() noexcept
{
	if(_queue == nullptr){
		return;
	}
	_queue->wait_for_resource_push(100);
	//循环这里只判断指针
	while(_queue != nullptr && _queue->has_data()){
		auto pack = _get_next_packet();
//		d_ptr->encode(pack.get());
	}
}

void AudioEncoder::on_thread_pause() noexcept
{
//	d_ptr->encode(nullptr);
}

bool AudioEncoder::get_thread_pause_condition() noexcept
{
	return _queue == nullptr;
}

/**
 * @brief get_next_packet
 * 从队列里获取下一个包
 * @return 
 * 如果失败则返回nullptr
 */
core::FramePacket::SharedPacket AudioEncoder::_get_next_packet() noexcept 
{
	std::lock_guard<std::mutex> lk(_mutex);
	//真正需要判断数据的语句放在锁里面
	if(_queue == nullptr || !_queue->has_data()){
		return nullptr;
	}
	return _queue->get_next();
}

} // namespace codec

} // namespace rtplivelib
