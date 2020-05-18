
#include "fectest.h"
#include "core/multioutputqueue.h"
#include "core/singleioqueue.h"
#include <gtest/gtest.h>

/**
 * 用于测试queue是否正常
 */

using namespace rtplivelib;
using namespace rtplivelib::core;

TEST(MultiOutputQueue,api){
	AbstractQueue<int> input;
	MultiOutputQueue<int> output1;
	AbstractQueue<int> output2;
	AbstractQueue<int> output3;
	
	ASSERT_FALSE(output1.has_output());
	ASSERT_FALSE(output1.has_input());
	ASSERT_FALSE(output1.is_input());
	ASSERT_FALSE(output1.is_output());
	output1.set_input(&output1);
	ASSERT_TRUE(output1.has_input());
	ASSERT_TRUE(output1.is_input());
	output1.set_input(&input);
	ASSERT_FALSE(output1.is_input());
	output1.insert_output(&output1);
	ASSERT_TRUE(output1.has_output());
	ASSERT_TRUE(output1.is_output());
	output1.insert_output(&output2);
	output1.insert_output(&output3);
	ASSERT_TRUE(output1.contain_output(&output2));
	ASSERT_TRUE(output1.contain_output(&output3));
	ASSERT_EQ(output1.get_output().size(),3);
	output1.remove_output(&output2);
	ASSERT_FALSE(output1.contain_output(&output2));
	ASSERT_EQ(output1.get_output().size(),2);
}

TEST(SingleIOQueue,api){
	SingleIOQueue<int> input;
	SingleIOQueue<int> output;
	
	input.set_input(&input);
	output.set_output(&output);
	ASSERT_FALSE(input.is_input());
	ASSERT_FALSE(input.has_input());
	ASSERT_FALSE(output.has_output());
	ASSERT_FALSE(output.is_output());
	input.set_output(&output);
	ASSERT_TRUE(input.has_output());
	ASSERT_TRUE(input.is_input());
	ASSERT_EQ(input.get_output(),&output);
	ASSERT_EQ(input.get_input(),&input);
	input.set_input(&output);
	ASSERT_TRUE(input.has_input());
	ASSERT_TRUE(input.is_output());
	ASSERT_EQ(input.get_output(),&input);
	ASSERT_EQ(input.get_input(),&output);
}
