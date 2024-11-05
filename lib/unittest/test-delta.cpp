#include <gtest/gtest.h>

#include "../delta.hpp"

namespace
{

auto MaxI = std::numeric_limits<chash::Index>::max();

//   S----|           |-----S
TEST(Delta, BoundCrossing)
{
	chash::DeltaBuilder<std::string> builder;
	builder.Add(chash::Slice{100, 10, 42});
	auto delta = builder.GetDelta().slices;

	const std::set<chash::Slice> expected{{0, 10, 42}, {100, MaxI, 42}};
	ASSERT_EQ(delta, expected);
}

/*
 *       |--------------------|
 *    |======|
 */
TEST(DeltaBuilder, ClipRight)
{
	chash::DeltaBuilder<std::string> builder;
	builder.Add(chash::Slice{10, 100, 42});
	builder.Add(chash::Slice{5, 25, 1});
	auto delta = builder.GetDelta().slices;

	const std::set<chash::Slice> expected{{5, 25, 1}, {25, 100, 42}};
	ASSERT_EQ(delta, expected);
}

/*
 *       |--------------------|
 *       |======|
 */
TEST(DeltaBuilder, ClipRightBorder)
{
	chash::DeltaBuilder<std::string> builder;
	builder.Add(chash::Slice{10, 100, 42});
	builder.Add(chash::Slice{10, 25, 1});
	auto delta = builder.GetDelta().slices;

	const std::set<chash::Slice> expected{{10, 25, 1}, {25, 100, 42}};
	ASSERT_EQ(delta, expected);
}

/*
 *       |--------------------|
 *       |===========================|
 */
TEST(DeltaBuilder, ShadowLeftBorder)
{
	chash::DeltaBuilder<std::string> builder;
	builder.Add(chash::Slice{10, 90, 42});
	builder.Add(chash::Slice{10, 100, 1});
	auto delta = builder.GetDelta().slices;

	const std::set<chash::Slice> expected{{10, 100, 1}};
	ASSERT_EQ(delta, expected);
}

/*
 *       |--------------------|
 *            |======|
 */
TEST(DeltaBuilder, ClipBoth)
{
	chash::DeltaBuilder<std::string> builder;
	builder.Add(chash::Slice{10, 100, 42});
	builder.Add(chash::Slice{25, 75, 1});
	auto delta = builder.GetDelta().slices;

	const std::set<chash::Slice> expected{{10, 25, 42}, {25, 75, 1}, {75, 100, 42}};
	ASSERT_EQ(delta, expected);
}

/*
 *       |-------|.........|
 *            |======|
 */
TEST(DeltaBuilder, ClipTwo)
{
	chash::DeltaBuilder<std::string> builder;
	builder.Add(chash::Slice{10, 50, 42});
	builder.Add(chash::Slice{50, 100, 43});
	builder.Add(chash::Slice{25, 75, 1});
	auto delta = builder.GetDelta().slices;

	const std::set<chash::Slice> expected{{10, 25, 42}, {25, 75, 1}, {75, 100, 43}};
	ASSERT_EQ(delta, expected);
}

/*
 *       |--------------------|
 *                     |======|
 */
TEST(DeltaBuilder, ClipLeftBorder)
{
	chash::DeltaBuilder<std::string> builder;
	builder.Add(chash::Slice{10, 100, 42});
	builder.Add(chash::Slice{75, 100, 1});
	auto delta = builder.GetDelta().slices;

	const std::set<chash::Slice> expected{{10, 75, 42}, {75, 100, 1}};
	ASSERT_EQ(delta, expected);
}

/*
 *       |--------------------|
 *   |========================|
 */
TEST(DeltaBuilder, ShadowRightBorder)
{
	chash::DeltaBuilder<std::string> builder;
	builder.Add(chash::Slice{10, 100, 42});
	builder.Add(chash::Slice{5, 100, 1});
	auto delta = builder.GetDelta().slices;

	const std::set<chash::Slice> expected{{5, 100, 1}};
	ASSERT_EQ(delta, expected);
}

/*
 *       |--------------------|
 *                         |======|
 */
TEST(DeltaBuilder, ClipLeft)
{
	chash::DeltaBuilder<std::string> builder;
	builder.Add(chash::Slice{10, 100, 42});
	builder.Add(chash::Slice{75, 110, 1});
	auto delta = builder.GetDelta().slices;

	const std::set<chash::Slice> expected{{10, 75, 42}, {75, 110, 1}};
	ASSERT_EQ(delta, expected);
}

/*
 *       |----|+++++++|---------|++++|-----|+++|-------|
 *            |================================|
 */
TEST(DeltaBuilder, ClipFull)
{
	chash::DeltaBuilder<std::string> builder;
	builder.Add(std::vector<chash::Slice>{
	        {5, 10, 42}, {10, 20, 43}, {20, 30, 42}, {30, 40, 43}, {40, 50, 42}, {50, 60, 43}, {60, 70, 42}});
	builder.Add(chash::Slice{10, 60, 1});
	auto delta = builder.GetDelta().slices;

	const std::set<chash::Slice> expected{{5, 10, 42}, {10, 60, 1}, {60, 70, 42}};
	ASSERT_EQ(delta, expected);
}

}