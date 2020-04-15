#include "fecencoder.h"
#include "codec/wirehair.h"

namespace rtplivelib {

namespace rtp_network {

namespace fec {

class FECEncoderPrivateData{
public:
	/*编码器*/
	Wirehair codec;
	
	FECEncoderPrivateData():
		codec(Wirehair::Encoder){
		
	}
};


///////////////////////////////////////////////

FECEncoder::FECEncoder():
	d_ptr(new FECEncoderPrivateData)
{
	set_symbol_size(1024);
}

FECEncoder::~FECEncoder()
{
	delete d_ptr;
}

void FECEncoder::set_symbol_size(uint32_t value) noexcept
{
	d_ptr->codec.set_packet_size(value);
}

uint32_t FECEncoder::get_symbol_size() noexcept
{
	return d_ptr->codec.get_packet_size();
}

core::Result FECEncoder::encode(core::FramePacket::SharedPacket packet,
								std::vector<std::vector<int8_t> > &output,
								FECParam & param) noexcept
{
	if(packet == nullptr)
		return core::Result::Invalid_Parameter;
	
	if(packet->size < d_ptr->codec.get_packet_size()){
		//如果编码数据太小，小于基本单位时，不编码，直接输出
		param.size = packet->size;
		param.flag = 0;
		param.repair_nb = 0;
		param.symbol_size = d_ptr->codec.get_packet_size();
		output.resize(1);
		std::vector<int8_t> d(param.size);
		memcpy(d.data(),packet->data[0],d.size());
		output[0] = d;
		return core::Result::Success;
	}
	
	float rate;
	if(packet->is_key())
		rate = 0.83f;
	else  
		rate = 0.9f;
	
	auto ret = d_ptr->codec.encode(packet->data[0],packet->size,rate,output);
	param.size = packet->size;
	param.symbol_size = d_ptr->codec.get_packet_size();
	param.flag = 1;
	return ret;
}

} //namespace fec

} //namespace rtp_network

} //namespace rtplivelib

