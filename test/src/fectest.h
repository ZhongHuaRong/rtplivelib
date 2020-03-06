
#pragma once 

#include "core/error.h"
#include <list>
#include <vector>
#include <set>
#include <time.h>
#include <fstream>
#include <memory>

/**
 * 该头文件设置了FEC测试需要用到的接口和参数
 */

static std::streamsize size = 1300;

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
static std::list<uint32_t> simulated_packet_loss(uint32_t src_nb,uint32_t repair_nb){
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

static std::pair<void **, void **> create_array(void ** data,uint32_t src_nb,uint32_t repair_nb) noexcept
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

static std::vector<std::ifstream::char_type> read_file(std::string name){
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

static void write_file(std::string name,void ** data,int nb,uint32_t size){
    std::ofstream file;
    file.open(name,std::ios_base::binary | std::ios_base::out);
    for(auto i = 0;i < nb;++i){
        file.write((char*)data[i],size);
        file.flush();
    }
    file.close();
}

static void write_file(std::string name,std::vector<int8_t> vec){
    std::ofstream file;
    file.open(name,std::ios_base::binary | std::ios_base::out);
    file.write((char*)vec.data(),vec.size());
    file.flush();
    file.close();
}
