#include <gtest/gtest.h>

#include "../utils.hpp"

namespace
{

TEST(RingIter, SizeOne)
{
	RingIterator it(1);
	ASSERT_TRUE(it == 0);
	ASSERT_TRUE(it + 1 == 0);
	ASSERT_TRUE(it - 1 == 0);
}

TEST(RingIter, Right)
{
	RingIterator i(42);
	ASSERT_TRUE(i + 41 == 41);
	ASSERT_TRUE(i + 42 == 0);
	RingIterator j(42, 41);
	ASSERT_TRUE(j + 1 == 0);
	ASSERT_TRUE(j + 5 == 4);

	ASSERT_TRUE(++RingIterator(42, 41) == 0);

	RingIterator k(42,41);
	k++;
	ASSERT_TRUE(k == 0);

	ASSERT_TRUE(++RingIterator(42) == RingIterator(42)++ + 1);
}

TEST(RingIter, Left)
{
	RingIterator i(42);
	ASSERT_TRUE(i - 1 == 41);
	RingIterator j(42, 41);
	ASSERT_TRUE(j - 1 == 40);
	ASSERT_TRUE(j - 6 == 35);
}

}