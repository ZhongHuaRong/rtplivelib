
#pragma once

#include "../core/abstractqueue.h"
#include "rtppacket.h"

namespace rtplivelib {

namespace rtp_network {

class RTPRecvThreadPrivateData;

class RTPLIVELIBSHARED_EXPORT RTPRecvThread:
		public core::AbstractQueue<RTPPacket>
{
public:
	RTPRecvThread();
	
	virtual ~RTPRecvThread() override;
protected:
	/**
	 * @brief on_thread_run
	 * 线程运行时需要处理的操作
	 */
	virtual void on_thread_run() noexcept override;
	
	/**
	 * @brief get_thread_pause_condition
	 * 只返回false，线程不存在暂停
	 */
	virtual bool get_thread_pause_condition() noexcept override;
private:
	RTPRecvThreadPrivateData * const d_ptr;
};

inline bool RTPRecvThread::get_thread_pause_condition() noexcept								{		return false;}

} // rtp_network

} // namespace rtplivelib
