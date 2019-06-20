#include "audioprocessingfactory.h"
#include <iostream>
#include <algorithm>
extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavdevice/avdevice.h>
	#include <libavutil/dict.h>
}

namespace rtplivelib {

namespace device_manager {


//////////////////////////////////////////////////////////////////////////////////////////

AudioProcessingFactory::AudioProcessingFactory()
{
	
}

}// namespace device_manager

} // namespace rtplivelib 

