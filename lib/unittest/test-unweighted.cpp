#include <gtest/gtest.h>

#include "common.h"

#include "../unweighted.hpp"

namespace
{

using namespace test;

constexpr auto SIZE = 8096;
constexpr auto SEED = 42;

TEST(Unweighted, Make)
{
	std::vector<std::string> reals = {"alpha", "beta", "gamma", "delta"};
	std::vector<std::uint32_t> ids = {1, 2, 3, 4};
	auto ring = chash::Unweighted<std::uint32_t>::Make(
		SIZE,
		ids.cbegin(),
		ids.cend(),
		reals.cbegin(),
		SEED
	);

	for (auto i = 0; i < SIZE; ++i)
	{
		auto a = ring.Match(i);
		auto a2 = ring.Match(i + SIZE);
		ASSERT_EQ(a, a2);
		ASSERT_GE(a, 1);
		ASSERT_LE(a, 4);
		ASSERT_NE(std::find(ids.begin(), ids.end(), a), ids.end());
	}

}

} // namespace