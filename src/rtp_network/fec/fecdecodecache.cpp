#include "fecdecodecache.h"
#include "jrtplib3/rtppacket.h"
#include "jrtplib3/rtpsourcedata.h"
#include "jrtplib3/rtpaddress.h"
#include "jrtplib3/rtcpcompoundpacket.h"
#include "jrtplib3/rtcpapppacket.h"
#include "jrtplib3/rtcpsdespacket.h"
#include "jrtplib3/rtpipv4address.h"
#include "../../core/logger.h"
extern "C"{
#include "libavcodec/avcodec.h"
//#include "libavutil/mem.h"
}

namespace rtplivelib {

namespace rtp_network {

namespace fec {

PackageVector::~PackageVector() 
{
	release_vector();
}

void PackageVector::release_vector() noexcept
{
	if(list != nullptr){
		uint16_t total = src_nb + repair_nb;
		for(auto n = 0u; n < total; ++ n){
			if( list[n] != nullptr)
				delete list[n];
		}
		free(list);
	}
	list = nullptr;
	ts = 0;
	src_nb = 0;
	repair_nb = 0;
	save_nb = 0;
}

bool PackageVector::init_vector(const uint32_t &ts,
								const uint16_t &src_nb, const uint16_t &repair_nb,
								const uint16_t& fill_size) noexcept
{
	release_vector();
	if( src_nb + repair_nb == 0)
		return false;
	list = static_cast<PackageData**>(calloc(src_nb + repair_nb, sizeof(PackageData*)));
	if(list == nullptr)
		return false;
	
	this->ts = ts;
	this->src_nb = src_nb;
	this->repair_nb = repair_nb;
	this->fill_size = fill_size;
	return true;
}

void PackageVector::insert_data(void *data, const uint64_t &len, const uint16_t &pos, RTPPacket *packet) noexcept
{
	auto& _d = list[pos];
	if(_d == nullptr)
		_d = new PackageData();
	
	if( _d == nullptr){
		delete packet;
		return;
	}
	_d->set_data(data,len,packet);
	save_nb +=1;
	if( pos >= src_nb)
		not_have_repair = false;
}

uint16_t PackageVector::get_block_size() noexcept
{
	if( list == nullptr)
		return 0u;
	
	for(auto n = 0u;n < src_nb + repair_nb; ++n){
		if(list[n] != nullptr)
			return list[n]->len;
	}
	
	return 0u;
}

std::pair<void *, uint64_t> PackageVector::copy_data(void **ptr, 
													 const uint16_t &total_nb,
													 const uint16_t &block_size, 
													 const uint16_t &fill_size) noexcept
{
	using pair = std::pair<void *,uint64_t>;
	
	if(ptr == nullptr)
		return pair(nullptr,0);
	
	auto size = (uint64_t)total_nb * (uint64_t)block_size - fill_size;
	void * data = av_malloc(size);
	if(data == nullptr)
		return pair(nullptr,0);
	
	uint8_t * _d = static_cast<uint8_t*>(data);
	//开始拷贝
	for( auto n = 0u; n < total_nb - 1u; ++n){
		//没有数据的情况下，填充0
		if( ptr[n] == nullptr)
			memset(_d,0,block_size);
		else
			memcpy(_d,ptr[n],block_size);
		_d += block_size;
	}
	if( ptr[total_nb - 1] == nullptr)
		memset(_d,0,block_size - fill_size);
	else
		memcpy(_d,ptr[total_nb - 1],block_size - fill_size);
	return pair(data,size);
}

std::pair<void *, uint64_t> PackageVector::copy_data(PackageVector *vector) noexcept
{
	using pair = std::pair<void *,uint64_t>;
	
	if(vector == nullptr || vector->list == nullptr)
		return pair(nullptr,0);
	
	uint64_t block_size = vector->get_block_size();
	
	size_t nb = vector->src_nb;
	
	auto size = block_size * nb - vector->fill_size;
	void * data = av_malloc(size);
	if(data == nullptr)
		return pair(nullptr,0);
	
	uint8_t * _d = static_cast<uint8_t*>(data);
	//开始拷贝
	for( auto n = 0u; n < nb - 1; ++n){
		//没有数据的情况下，填充0
		if( vector->list[n] == nullptr || vector->list[n]->data == nullptr)
			memset(_d,0,block_size);
		else
			memcpy(_d,vector->list[n]->data,block_size);
		_d += block_size;
	}
	
	if( vector->list[nb - 1] == nullptr || vector->list[nb - 1]->data == nullptr)
		memset(_d,0,block_size - vector->fill_size);
	else
		memcpy(_d,vector->list[nb - 1]->data,block_size - vector->fill_size);
	return pair(data,size);
}

std::pair<void **, void **> PackageVector::create_array() noexcept
{
	if(list == nullptr){
		return std::pair<void **, void **>(nullptr,nullptr);
	}
	uint16_t total = src_nb + repair_nb;
	auto first_ptr = static_cast<void **>(calloc(total,sizeof(void*)));
	if(first_ptr == nullptr)
		return std::pair<void **, void **>(nullptr,nullptr);
	
	auto second_ptr = static_cast<void **>(calloc(total,sizeof(void*)));
	if(second_ptr == nullptr){
		free(first_ptr);
		return std::pair<void **, void **>(nullptr,nullptr);
	}
	
	for(auto n = 0u; n < total; ++ n){
		if( list[n] != nullptr)
			first_ptr[n] = list[n]->data;
		else
			first_ptr[n] = nullptr;
		second_ptr[n] = nullptr;
	}
	return std::pair<void **, void **>(first_ptr,second_ptr);
}

void PackageVector::release_decode_array(std::pair<void **, void **> pair) noexcept
{
	auto &first_ptr = pair.first;
	auto &second_ptr = pair.second;
	
	if( first_ptr == nullptr){
		if(second_ptr != nullptr)
			free(second_ptr);
		return;
	}
	
	if( second_ptr == nullptr){
		if(first_ptr != nullptr)
			free(first_ptr);
		return;
	}
	
	for(auto n = 0u; n < src_nb; ++n){
		if( first_ptr[n] == nullptr)
			free(second_ptr[n]);
	}
	free(first_ptr);
	free(second_ptr);
}

//////////////////////////////////////////////////////////////////////

struct CallBack{
public:
	void operator () (FECDecodeCache* cache) {
		cache->lock_source();
		cache->remove_first();
		core::Logger::Print("remove","",LogLevel::ERROR_LEVEL);
		cache->unlock_source();
	}
};

FECDecodeCache::FECDecodeCache():
	timer([&](){
		CallBack cb;
		cb(this);
	})
{
	timer.set_loop(true);
}

bool FECDecodeCache::pop(void *data, const uint64_t &len,
						 const uint32_t &timestamp, 
						 const uint16_t &src_nb,
						 const uint16_t &repair_nb, 
						 const uint16_t &fill_size,
						 const uint16_t &pos,
						 const int &payload_type,
						 RTPPacket *packet) noexcept
{
	cur_pt = static_cast<RTPSession::PayloadType>(payload_type);
	
	auto i = insert(data,len,timestamp,src_nb,repair_nb,fill_size,pos,packet);
	
	if(i == nullptr)
		return false;
	return i->is_finish();
}

uint32_t FECDecodeCache::pop(RTPPacket *rtp_packet) noexcept
{
	jrtplib::RTPPacket * packet = static_cast<jrtplib::RTPPacket*>(rtp_packet->get_packet());
	bool ret;
	if(packet->GetExtensionData() != nullptr){
		uint64_t extension_data;
		memcpy(&extension_data,packet->GetExtensionData(),8);
		ret = pop(packet->GetPayloadData(),packet->GetPayloadLength(),
				  packet->GetTimestamp(),
				  static_cast<uint16_t>( (extension_data >> 48 ) & 0x000000000000FFFF ),
				  static_cast<uint16_t>( (extension_data >> 32 ) & 0x000000000000FFFF ),
				  static_cast<uint16_t>( extension_data & 0x000000000000FFFF ),
				  packet->GetExtensionID(),
				  packet->GetPayloadType(),
				  rtp_packet);
	}
	else
		ret = pop(packet->GetPayloadData(),packet->GetPayloadLength(),
				  packet->GetTimestamp(),
				  1,
				  0,
				  0,
				  0,
				  packet->GetPayloadType(),
				  rtp_packet);
	
	if(ret == true)
		return get_correct_timestamp(packet->GetTimestamp(),packet->GetExtensionID());
	else
		return 0;
}

uint32_t FECDecodeCache::get_correct_timestamp(const uint32_t &ts, const uint16_t &pos) noexcept
{
	return ts - pos;
}

PackageVector *FECDecodeCache::take_at(const uint32_t &ts) noexcept
{
	auto i = list.find(ts);
	if( i == list.end())
		return nullptr;
	
	//移除以前的所有数据
	auto it = list.begin();
	auto next = it;
	for(;it != list.end() && it != i;it = next){
		if(it->second != nullptr)
			delete it->second;
		next = it;
		++next;
		list.erase(it);
	}
	
	auto ptr = i->second;
	list.erase(i);
	min_timestamp = ptr->ts + 1;
	return ptr;
}

PackageVector *FECDecodeCache::push() noexcept
{
	if(list.empty())
		return nullptr;
	
	auto & ptr = list.begin()->second;
	if(ptr == nullptr)
		return nullptr;
	
	if(ptr->is_finish()){
		list.erase(list.begin());
		min_timestamp = ptr->ts + 1;
		return ptr;
	}
	return nullptr;
}

void FECDecodeCache::remove_first() noexcept
{
	if(list.empty())
		return;
	
	auto & ptr = list.begin()->second;
	
	list.erase(list.begin());
	if(ptr != nullptr){
		min_timestamp = ptr->ts + 1;
		delete ptr;
	}
}

void FECDecodeCache::lock_source() noexcept
{
	mutex.lock();
}

void FECDecodeCache::unlock_source() noexcept
{
	mutex.unlock();
}

PackageVector *FECDecodeCache::insert(void *data, const uint64_t &len,
									  const uint32_t &ts, 
									  const uint16_t &src_nb,
									  const uint16_t &repair_nb,
									  const uint16_t &fill_size,
									  const uint16_t &pos, 
									  RTPPacket *packet) noexcept
{
	auto _ts = get_correct_timestamp(ts,pos);
	
	//如果ts比最小的ts还要低则不添加了
	if( _ts < min_timestamp){
		delete packet;
		return nullptr;
	} else if( list.empty() == true || _ts == list.begin()->first ){
		if(!timer.is_running())
			timer.start(wait_time);
		else
			timer.restart();
	}
	
	auto& i = list[_ts];
	
	
	if(i == nullptr){
		i = new PackageVector;
		
		if(i == nullptr || i->init_vector(ts,src_nb,repair_nb,fill_size) == false){
			delete packet;
			return nullptr;
		}
	}
	
	i->insert_data(data,len,pos,packet);
	
	core::Logger::Print("ts:{},pos:{},src_nb:{}","",LogLevel::INFO_LEVEL,ts,pos,src_nb);
	return i;
}


} //namespace fec

} //namespace rtp_network

} //namespace rtplivelib
