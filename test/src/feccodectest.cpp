
#include "rtp_network/fec/fecdecoder.h"
#include "rtp_network/fec/fecencoder.h"
#include <gtest/gtest.h>
#include "fectest.h"

using namespace rtplivelib;
using namespace rtplivelib::rtp_network;
using namespace rtplivelib::core;
using namespace rtplivelib::rtp_network::fec;

TEST(FECTest,RS){
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

TEST(FECTest,LDPC){
    FECEncoder encoder;
    FECDecoder decoder;
    
    encoder.set_code_rate(0.8f);
    encoder.set_symbol_size(size);
    
    auto data_vector = read_file("config.log");
    bool ret{false};
    auto encode_size = data_vector.size();
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
            ASSERT_EQ(memcmp(data_ptr[i],pack.second[i],size),0) << "i:" << i << ",count:" << ++count <<
                                                                    ",repair:" << encoder.get_repair_nb();
        }
        write_file("./debug/1.txt",pack.second,src_nb,size);
    }
}
