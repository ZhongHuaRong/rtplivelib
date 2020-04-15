
#pragma once

#include "../core/config.h"
#include "../core/timer.h"

namespace rtplivelib {

namespace rtp_network {

/**
 * @brief The RTPBandwidth class
 * 用来统计网络流量,每秒的网速
 */
class RTPLIVELIBSHARED_EXPORT RTPBandwidth
{
public:
	template<typename CallBack>
	RTPBandwidth(CallBack cb):
		timer([&cb,this](){
			cb(get_speed(),get_total());
			reset_speed();})
	{
		timer.set_loop(true);
		timer.start(1000);
	}
	
	~RTPBandwidth(){
		
	}
	
	inline void add_value(uint64_t value) noexcept {
		_speed += value;
		_total_flow += value;
	}
	
	inline uint64_t get_speed() const noexcept {
		return _speed;
	}
	
	inline uint64_t get_total() const noexcept {
		return _total_flow;
	}
	
	inline void reset_speed() noexcept {
		_speed = 0;
	}
private:
	volatile uint64_t	_speed{0};
	volatile uint64_t	_total_flow{0};
	core::Timer			timer;
};

} // namespace rtp_network

} // namespace rtplivelib
