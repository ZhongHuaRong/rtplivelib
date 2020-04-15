#include "alsa.h"
#include "../core/logger.h"
#include "../core/except.h"
#include <alsa/asoundlib.h>
#include "../core/format.h"
#include "../core/time.h"

namespace rtplivelib {

namespace device_manager{

class ALSAPrivateData{
public:
	std::recursive_mutex	mutex;
	ALSA::FlowType			current_flow_type{ALSA::FlowType::RENDER};
	std::string				current_name{"default"};
	snd_pcm_t				*handle{nullptr};
	snd_pcm_hw_params_t		*params{nullptr};
	
	core::Format			format;
	snd_pcm_uframes_t		frames{512};
	int						buffer_size{0};
};

ALSA::ALSA():
	d_ptr(new ALSAPrivateData())
{
	/*在子类初始化里开启线程*/
	start_thread();
}

ALSA::~ALSA()
{
	stop();
	exit_wait_resource();
	exit_thread();
	if(d_ptr->params != nullptr)
		snd_pcm_hw_params_free(d_ptr->params);
	delete d_ptr;
}

std::vector<ALSA::device_info> ALSA::get_all_device_info(ALSA::FlowType ft)
{
	void ** hints{nullptr};
	int ret = snd_device_name_hint(-1,"pcm",&hints);
	if(ret < 0)
		throw std::runtime_error("unknown error");
	
	const char * type = ft == ALSA::CAPTURE?"Input":"Output";
	char *name{nullptr},*desc{nullptr},*io{nullptr};
	void ** i = hints;
	std::vector<ALSA::device_info> info_vector;
	while(*i != nullptr){
		name = snd_device_name_get_hint(*i,"NAME");
		desc = snd_device_name_get_hint(*i,"DESC");
		io = snd_device_name_get_hint(*i,"IOID");
		
		if( io != nullptr && strcmp(io,type) == 0){
			ALSA::device_info pair;
			pair.first = name;
			pair.second = pair.first;
			info_vector.push_back(pair);
		}
		
		free(name);
		free(desc);
		free(io);
		++i;
	}
	snd_device_name_free_hint(hints);
	return info_vector;
}

ALSA::device_info ALSA::get_current_device_info() noexcept
{
	return ALSA::device_info(d_ptr->current_name,d_ptr->current_name);
}

bool ALSA::set_current_device(uint64_t num, ALSA::FlowType ft) noexcept
{
	try {
		auto list = get_all_device_info(ft);
		if( list.size() <= num )
			return false;
		
		return set_current_device(list[num].first,ft);
	} catch (...) {
		return false;
	}
	return false;
}

bool ALSA::set_current_device(const std::string &name, ALSA::FlowType ft) noexcept
{
	UNUSED(ft)
			stop();
	std::lock_guard<decltype(d_ptr->mutex)> lk(d_ptr->mutex);
	d_ptr->current_name = name;
	return true;
}

bool ALSA::set_default_device(ALSA::FlowType ft) noexcept
{
	return set_current_device("default",ft);
}

void ALSA::set_format(const core::Format &format) noexcept
{
	std::lock_guard<decltype(d_ptr->mutex)> lk(d_ptr->mutex);
	d_ptr->format = format;
}

const core::Format ALSA::get_format() noexcept
{
	return d_ptr->format;
}

bool ALSA::start() noexcept
{
	std::lock_guard<decltype(d_ptr->mutex)> lk(d_ptr->mutex);
	if(d_ptr->handle != nullptr)
		stop();
	int rc = snd_pcm_open(&d_ptr->handle, d_ptr->current_name.c_str(),
						  SND_PCM_STREAM_CAPTURE, 0);
	if (rc < 0) {
		core::Logger::Print("unable to open pcm device:{}",
							__PRETTY_FUNCTION__,
							LogLevel::WARNING_LEVEL,
							snd_strerror(rc));
		return false;
	}
	if(d_ptr->params == nullptr){
		snd_pcm_hw_params_malloc(&d_ptr->params);
		if(d_ptr->params == nullptr){
			return false;
		}
	}
	
	/* 填充默认参数 */
	rc = snd_pcm_hw_params_any(d_ptr->handle, d_ptr->params);
	if (rc < 0) {
		core::Logger::Print("{}",
							__PRETTY_FUNCTION__,
							LogLevel::WARNING_LEVEL,
							snd_strerror(rc));
		return false;
	}
	
	/* 将使用默认参数， */
	
	/* Interleaved mode */
	snd_pcm_hw_params_set_access(d_ptr->handle, d_ptr->params,
								 SND_PCM_ACCESS_RW_INTERLEAVED);
	
	int dir;
	//如果设置了格式，则需要初始化参数
	//不过目前没有设置参数的接口
	if(d_ptr->format != core::Format()){
		switch (d_ptr->format.bits) {
		case 8:
			snd_pcm_hw_params_set_format(d_ptr->handle, d_ptr->params,
										 SND_PCM_FORMAT_S8);
			break;
		case 16:
			snd_pcm_hw_params_set_format(d_ptr->handle, d_ptr->params,
										 SND_PCM_FORMAT_S16_LE);
			break;
		case 32:
			snd_pcm_hw_params_set_format(d_ptr->handle, d_ptr->params,
										 SND_PCM_FORMAT_FLOAT_LE);
			break;
		case 64:
			snd_pcm_hw_params_set_format(d_ptr->handle, d_ptr->params,
										 SND_PCM_FORMAT_FLOAT64_LE);
			break;
		}
		snd_pcm_hw_params_set_channels(d_ptr->handle, d_ptr->params, d_ptr->format.channels);
		uint val = d_ptr->format.sample_rate;
		snd_pcm_hw_params_set_rate_near(d_ptr->handle, d_ptr->params,&val, &dir);
	}
	
	/* Set period size */
	d_ptr->frames = 512;
	snd_pcm_hw_params_set_period_size_near(d_ptr->handle, d_ptr->params,
										   &d_ptr->frames, &dir);
	
	/* Write the parameters to the driver */
	rc = snd_pcm_hw_params(d_ptr->handle, d_ptr->params);
	if (rc < 0) {
		fprintf(stderr,
				"unable to set hw parameters: %s\n",
				snd_strerror(rc));
		return false;
	}
	
	snd_pcm_hw_params_get_period_size(d_ptr->params,&d_ptr->frames, &dir);
	
	//如果使用的是默认格式，则需要初始化format
	if(d_ptr->format == core::Format()){
		uint32_t val;
		snd_pcm_hw_params_get_channels(d_ptr->params,&val);
		d_ptr->format.channels = static_cast<int>(val);
		
		snd_pcm_format_t fmt;
		snd_pcm_hw_params_get_format(d_ptr->params,&fmt);
		
		switch (fmt) {
		case SND_PCM_FORMAT_S8:
			d_ptr->format.bits = 8;
			break;
		case SND_PCM_FORMAT_S16_LE:
		case SND_PCM_FORMAT_S16_BE:
			d_ptr->format.bits = 16;
			break;
		case SND_PCM_FORMAT_FLOAT_LE:
		case SND_PCM_FORMAT_FLOAT_BE:
			d_ptr->format.bits = 32;
			break;
		case SND_PCM_FORMAT_FLOAT64_LE:
		case SND_PCM_FORMAT_FLOAT64_BE:
			d_ptr->format.bits = 64;
			break;
		default:
			d_ptr->format.bits = 0;
		}
		snd_pcm_hw_params_get_rate(d_ptr->params,&val,&dir);
		//好像会比正常的少1
		d_ptr->format.sample_rate = val;
	}
	
	//设置缓冲区大小
	d_ptr->buffer_size = d_ptr->frames * d_ptr->format.bits / 8 * d_ptr->format.channels;
	_is_running_flag = true;
	notify_thread();
	//防止外部调用正在使用read_packet接口
	exit_wait_resource();
	return true;
}

bool ALSA::stop() noexcept
{
	std::lock_guard<decltype(d_ptr->mutex)> lk(d_ptr->mutex);
	if(d_ptr->handle != nullptr){
		snd_pcm_drain(d_ptr->handle);
		snd_pcm_close(d_ptr->handle);
		d_ptr->handle = nullptr;
	}
	_is_running_flag = false;
	return true;
}

bool ALSA::is_start() noexcept
{
	return _is_running_flag;
}

core::FramePacket::SharedPacket ALSA::read_packet() noexcept
{
	wait_resource_push();
	
	return get_next();
}

void ALSA::on_thread_run() noexcept
{
	sleep(1);
	std::lock_guard<decltype(d_ptr->mutex)> lk(d_ptr->mutex);
	if(d_ptr->handle == nullptr || !_is_running_flag){
		stop();
		return ;
	}
	
	auto buffer = static_cast<uint8_t*>(malloc(d_ptr->buffer_size));
	if( buffer == nullptr){
		core::Logger::Print_APP_Info(core::Result::FramePacket_data_alloc_failed,
									 __PRETTY_FUNCTION__,
									 LogLevel::WARNING_LEVEL);
		return;
	}
	
	auto rc = snd_pcm_readi(d_ptr->handle, buffer, d_ptr->frames);
	if (rc == -EPIPE) {
		/* EPIPE means overrun */
		snd_pcm_prepare(d_ptr->handle);
	} else if (rc < 0) {
		core::Logger::Print("error from read:{}",
							__PRETTY_FUNCTION__,
							LogLevel::WARNING_LEVEL,
							snd_strerror(rc));
		return ;
	}
	
	auto packet = core::FramePacket::Make_Shared();
	if(packet == nullptr){
		core::Logger::Print_APP_Info(core::Result::FramePacket_data_alloc_failed,
									 __PRETTY_FUNCTION__,
									 LogLevel::WARNING_LEVEL);
		free(buffer);
		return;
	}
	packet->data[0] = buffer;
	
	packet->size = d_ptr->buffer_size;
	packet->format = get_format();
	//获取时间戳
	packet->dts = core::Time::Now().to_timestamp();
	packet->pts = packet->dts;
	push_one(packet);
}

void ALSA::on_thread_pause() noexcept
{
	stop();
}

} // namespace device_manager

} // namespace rtplivelib
