#include <algorithm>
#include <vector>

#include <gtest/gtest.h>

#include "../bit-reverse.hpp"

namespace
{

TEST(BitReverse, VisitsAll)
{
	static constexpr std::uint32_t Bits = 18;
	static constexpr std::size_t Size = 1 << Bits;
	std::vector<bool> seen(Size, false);

		for (std::uint32_t i = 0, pos = 0; i < Size; ++i, pos = ReverseBits<Bits>(i))
	{
		ASSERT_FALSE(seen.at(pos));
		seen[pos] = true;
	}

	ASSERT_EQ(std::find(seen.begin(), seen.end(), false), seen.end());
}

}
