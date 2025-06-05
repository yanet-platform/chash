#include <gtest/gtest.h>

#include "common.h"

#include "../chash.hpp"

namespace
{

using namespace test;

constexpr auto CELLS_PER_WEIGHT_UNIT = 40;
constexpr auto REAL_MAPPINGS_LIMIT = 1000;
constexpr auto MARKER = 255;
constexpr auto PADDING = 100;

TEST(Speed, Make)
{
	ASSERT_TRUE(true);
}

} // namespace