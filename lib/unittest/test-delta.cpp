#include <gtest/gtest.h>

#include "../delta.hpp"

namespace
{

auto MaxI = std::numeric_limits<chash::Index>::max();

TEST(Delta, BoundCrossing)
{
	chash::DeltaBuilder<std::string> builder;
	builder.Add(chash::Slice{100, 10, 42});
	auto delta = builder.GetDelta().slices;


	const std::set<chash::Slice> expected{{0, 10, 42}, {100, MaxI, 42}};
	ASSERT_EQ(delta, expected);
}


}