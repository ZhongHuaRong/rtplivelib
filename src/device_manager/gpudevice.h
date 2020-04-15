
#pragma once

#include "../core/config.h"
#include <string>

//GPU设备
//用于处理显卡信息
struct GPUDevice{
	std::string		name;
	uint32_t		id;
	uint64_t		video_memory;
	uint64_t		system_memory;
	uint64_t		shared_system_memory;
};

#ifdef WIN64
#include <dxgi.h>
#include <windows.h>

class RTPLIVELIBSHARED_EXPORT DXIG_GPU{
	
};


#endif
