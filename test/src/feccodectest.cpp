
#include "rtp_network/fec/fecdecoder.h"
#include "rtp_network/fec/fecencoder.h"
#include <gtest/gtest.h>
#include <fstream>
#include <time.h>
#include <list>

/**
 * 用于测试FEC的编解码是否正常
 */

using namespace rtplivelib;
using namespace rtplivelib::rtp_network;
using namespace rtplivelib::core;
using namespace rtplivelib::rtp_network::fec;

static std::streamsize size = 1024;

/**
 * @brief simulated_packet_loss
 * 用于模拟丢包，将随机去除冗余包数量的包，返回包含包号的数组
 * @param total_nb
 * 编码所获到的总包数，即原包数+冗余包数
 * @param code_rate
 * 数据占比数
 * @return
 * 
 */
std::list<uint32_t> simulated_packet_loss(uint32_t src_nb,uint32_t repair_nb){
    srand((unsigned int)time(nullptr));
    uint32_t total = src_nb + repair_nb;
    std::set<uint32_t> pk;
    for(auto i = 0u;i < total;++i){
        pk.insert(i);
    }
    while(pk.size() > src_nb + 10){
        pk.erase(rand() % total);
    }
    return std::list<uint32_t>(pk.begin(),pk.end());
}

std::pair<void **, void **> create_array(void ** data,uint32_t src_nb,uint32_t repair_nb) noexcept
{
	uint32_t total = src_nb + repair_nb;
	auto first_ptr = static_cast<void **>(calloc(total,sizeof(void*)));
	if(first_ptr == nullptr)
		return std::pair<void **, void **>(nullptr,nullptr);
	
	auto second_ptr = static_cast<void **>(calloc(total,sizeof(void*)));
	if(second_ptr == nullptr){
		free(first_ptr);
		return std::pair<void **, void **>(nullptr,nullptr);
	}
	
    auto pack_num_list = simulated_packet_loss(src_nb,repair_nb);
    
    for(auto n = 0u; n < total; ++ n){
        first_ptr[n] = nullptr;
		second_ptr[n] = nullptr;
	}
    for(auto i = pack_num_list.begin();i != pack_num_list.end();++i){
        first_ptr[*i] = data[*i];
    }
	return std::pair<void **, void **>(first_ptr,second_ptr);
}

std::vector<std::ifstream::char_type> read_file(std::string name){
    std::ifstream file;
    std::string file_path(std::string("./debug/") + name);
    file.open(file_path,std::ios_base::binary | std::ios_base::in);
    
    if(!file.is_open()){
        file_path = "./";
        file_path += name;
        file.open(file_path,std::ios_base::binary);
    }
    
    if(!file.is_open())
        return std::vector<std::ifstream::char_type>();
    
    std::ifstream::char_type * buf{nullptr};
    buf = new std::ifstream::char_type[size];
    
    if(buf == nullptr)
        return std::vector<std::ifstream::char_type>();
    
    //用于自动释放资源
    std::shared_ptr<std::ifstream::char_type> ptr(buf,[&](std::ifstream::char_type* p){
        file.close();
        delete []p;
    });
    
    std::streamsize read_size {0};
    std::vector<std::ifstream::char_type> data_vector;
    
    read_size = file.readsome(buf,size);
    while(read_size > 0){
        data_vector.insert(data_vector.end(),buf,buf + read_size);
        read_size = file.readsome(buf,size);
    }
    return data_vector;
}

void write_file(std::string name,void ** data,int nb,uint32_t size){
    std::ofstream file;
    file.open(name,std::ios_base::binary | std::ios_base::out);
    for(auto i = 0;i < nb;++i){
        file.write((char*)data[i],size);
        file.flush();
    }
    file.close();
}

TEST(FECTest,LDPCCodec_RS){
    FECEncoder encoder;
    FECDecoder decoder;
    
    encoder.set_code_rate(0.8f);
    encoder.set_symbol_size(size);
    
    auto data_vector = read_file("config.log");
    bool ret{false};
    auto encode_size = data_vector.size() < size * 255 * encoder.get_code_rate()?
                data_vector.size():size * 255* encoder.get_code_rate();
    ret = encoder.encode(data_vector.data(),encode_size);
    ASSERT_TRUE(ret);
    
    auto data_ptr = encoder.get_data();
    auto src_nb = encoder.get_all_pack_nb() - encoder.get_repair_nb();
    for(auto n = 0;n < 1;++n){
        auto pack = create_array(data_ptr,src_nb,encoder.get_repair_nb());
        
        ret = decoder.decode(src_nb,
                             encoder.get_repair_nb(),
                             size,
                             pack.first,
                             pack.second);
        ASSERT_TRUE(ret);
        int count{0};
        for(auto i = 0u;i < src_nb;++i){
            EXPECT_EQ(memcmp(data_ptr[i],pack.second[i],size),0) << "i:" << i << ",count:" << ++count <<
                                                                    ",repair:" << encoder.get_repair_nb();
        }
        write_file("./debug/2.txt",pack.second,src_nb,size);
    }
}
