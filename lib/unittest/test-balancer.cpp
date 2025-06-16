#include <gtest/gtest.h>

#include "common.h"

#include "../balancer.hpp"

namespace
{

using namespace std::chrono_literals;
using namespace test;

TEST(Balancer, Construct)
{
	chash::Balancer balancer;
	ASSERT_EQ(balancer.size(), 0);
	ASSERT_TRUE(balancer.empty());

	std::vector<std::string> reals{
	        "2a02:6b8:c0e:1003:0:675:a15a:3314",
	        "2a02:6b8:c0e:1003:0:675:a15a:3ca0",
	        "2a02:6b8:c0e:1003:0:675:a15a:4174",
	        "2a02:6b8:c0e:1003:0:675:a15a:4bb8",
	        "2a02:6b8:c0e:1003:0:675:a15a:4d6c",
	        "2a02:6b8:c0e:1003:0:675:a160:e98"};
	std::vector<std::uint32_t> ids{113, 114, 115, 116, 117, 118};
	std::vector<std::uint32_t> weights(ids.size(), 0);
	weights[0] = 1;

	balancer.AddService(1, ids.begin(), ids.end(), reals.begin(), weights.begin());
	ASSERT_EQ(balancer.size(), 1);
	ASSERT_FALSE(balancer.empty());

	auto [b, e] = balancer.Lookup(1);
	ASSERT_GT(std::distance(b, e), 0);
	ASSERT_TRUE(std::none_of(b, e, [](auto e) { return e != 113; }));

	weights[0] = 0;
	balancer.UpdateWeights(1, ids.begin(), ids.end(), weights.begin());
	std::this_thread::sleep_for(2ms);

	auto [b1, e1] = balancer.Lookup(1);
	ASSERT_EQ(std::distance(b1, e1), 0);

	weights[1] = 1;
	balancer.UpdateWeights(1, ids.begin(), ids.end(), weights.begin());
	std::this_thread::sleep_for(2ms);

	auto [b2, e2] = balancer.Lookup(1);
	ASSERT_EQ(std::distance(b2, e2), std::distance(b, e));
	ASSERT_TRUE(std::none_of(b2, e2, [](auto e) { return e != 114; }));

	weights[1] = 0;
	weights[2] = 1;
	balancer.UpdateWeights(1, ids.begin(), ids.end(), weights.begin());
	std::this_thread::sleep_for(2ms);

	{
		auto [b2, e2] = balancer.Lookup(1);
		ASSERT_EQ(std::distance(b2, e2), std::distance(b, e));
		ASSERT_TRUE(std::none_of(b2, e2, [](auto e) { return e != 115; }));
	}
	// std::this_thread::sleep_for(2000ms);
}

TEST(Balancer, Fuzz)
{
	chash::Balancer balancer;
	ASSERT_EQ(balancer.size(), 0);
	ASSERT_TRUE(balancer.empty());

	std::vector<std::string> reals{
	        "2a02:6b8:c0e:1003:0:675:a15a:3314",
	        "2a02:6b8:c0e:1003:0:675:a15a:3ca0",
	        "2a02:6b8:c0e:1003:0:675:a15a:4174",
	        "2a02:6b8:c0e:1003:0:675:a15a:4bb8",
	        "2a02:6b8:c0e:1003:0:675:a15a:4d6c",
	        "2a02:6b8:c0e:1003:0:675:a160:e98"};
	std::vector<std::uint32_t> ids{113, 114, 115, 116, 117, 118};
	std::vector<std::uint32_t> weights(ids.size(), 0);
	weights[0] = 1;

	std::default_random_engine e1(42);
	std::uniform_int_distribution<int> uni_rand_weight(0, 100);

	balancer.AddService(1, ids.begin(), ids.end(), reals.begin(), weights.begin());
	chash::Balancer balancer2;
	for (int i = 0; i < 100; ++i)
	{
		balancer.UpdateWeights(1, ids.begin(), ids.end(), weights.begin());
		balancer2.AddService(1, ids.begin(), ids.end(), reals.begin(), weights.begin());
		std::this_thread::sleep_for(1ms);

		auto [b1, e1] = balancer.Lookup(1);
		auto [b2, e2] = balancer2.Lookup(1);

		auto [m1, m2] = std::mismatch(b1, e1, b2, e2);
		ASSERT_EQ(std::distance(b1,m1), std::distance(b1, e1));

		balancer2.RemoveService(1);
	}

}

} // namespace