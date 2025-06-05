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

TEST(Basic, EmptyToOne)
{
	std::vector<std::string> reals{
	        "2a02:6b8:c0e:1003:0:675:a15a:3314",
	        "2a02:6b8:c0e:1003:0:675:a15a:3ca0",
	        "2a02:6b8:c0e:1003:0:675:a15a:4174",
	        "2a02:6b8:c0e:1003:0:675:a15a:4bb8",
	        "2a02:6b8:c0e:1003:0:675:a15a:4d6c",
	        "2a02:6b8:c0e:1003:0:675:a160:e98"};
	std::vector<std::uint32_t> ids{113, 114, 115, 116, 117, 118};
	std::vector<std::uint32_t> weights(ids.size(), 0);

	auto rsize = chash::WeightUpdater::LookupRequiredSize(
	        ids.size(), CELLS_PER_WEIGHT_UNIT);

	auto opt = chash::WeightUpdater::MakeWeightUpdater(
	        reals.data(),
	        ids.data(),
	        weights.data(),
	        ids.size(),
	        REAL_MAPPINGS_LIMIT,
	        CELLS_PER_WEIGHT_UNIT,
	        rsize);
	ASSERT_TRUE(opt);

	auto& updater = opt.value();

	std::vector<std::uint32_t> lookup(rsize + 2 * PADDING, MARKER);

	updater.InitLookup(lookup.data() + PADDING);
	ASSERT_TRUE(std::none_of(lookup.begin(), lookup.begin() + PADDING, [](auto value) {
		return value != MARKER;
	}));

	ASSERT_TRUE(std::none_of(lookup.begin() + PADDING, lookup.end() - PADDING, [](auto value) {
		return value != std::numeric_limits<std::uint32_t>::max();
	}));

	ASSERT_TRUE(std::none_of(lookup.end() - PADDING, lookup.end(), [](auto value) {
		return value != MARKER;
	}));

	const auto single = ids.at(0);

	std::uint32_t uid{single};
	std::uint32_t uweight{1};
	updater.UpdateLookupOneByOne(&uid, &uweight, 1, lookup.data() + PADDING);

	ASSERT_TRUE(std::none_of(lookup.begin(), lookup.begin() + PADDING, [](auto value) {
		return value != MARKER;
	}));

	ASSERT_TRUE(std::none_of(lookup.begin() + PADDING, lookup.end() - PADDING, [single](auto value) {
		return value != single;
	}));

	ASSERT_TRUE(std::none_of(lookup.end() - PADDING, lookup.end(), [](auto value) {
		return value != MARKER;
	}));

	std::fill(weights.begin(), weights.end(), 1);

	updater.UpdateLookupOneByOne(ids.data(), weights.data(), ids.size(), lookup.data() + PADDING);

	ASSERT_TRUE(std::none_of(lookup.begin(), lookup.begin() + PADDING, [](auto value) {
		return value != MARKER;
	}));

	ASSERT_TRUE(std::none_of(lookup.begin() + PADDING, lookup.end() - PADDING, [&](auto value) {
		return std::find(ids.begin(), ids.end(), value) == ids.end();
	}));

	std::unordered_map<std::uint32_t, std::uint32_t> dist;
	for (auto i = lookup.cbegin() + PADDING, end = lookup.cend() - PADDING; i != end; ++i)
	{
		++dist[*i];
	}

	ASSERT_TRUE(std::none_of(dist.begin(), dist.end(), [](const auto& value) {
		return value.second < CELLS_PER_WEIGHT_UNIT;
	}));

	ASSERT_TRUE(std::none_of(lookup.end() - PADDING, lookup.end(), [](auto value) {
		return value != MARKER;
	}));

	uid = 116;
	for (int i = 0; i < 5; ++i)
	{
		uweight = 0;
		updater.UpdateLookupOneByOne(&uid, &uweight, 1, lookup.data() + PADDING);

		ASSERT_TRUE(std::none_of(lookup.begin(), lookup.begin() + PADDING, [](auto value) {
			return value != MARKER;
		}));

		ASSERT_TRUE(std::none_of(lookup.begin() + PADDING, lookup.end() - PADDING, [&](auto value) {
			return std::find(ids.begin(), ids.end(), value) == ids.end();
		}));

		dist.clear();
		for (auto i = lookup.cbegin() + PADDING, end = lookup.cend() - PADDING; i != end; ++i)
		{
			++dist[*i];
		}

		ASSERT_EQ(dist[uid], 0);

		ASSERT_TRUE(std::none_of(dist.begin(), dist.end(), [&](const auto& value) {
			if (value.first == uid)
			{
				return false;
			}
			return value.second < CELLS_PER_WEIGHT_UNIT;
		}));

		ASSERT_TRUE(std::none_of(lookup.end() - PADDING, lookup.end(), [](auto value) {
			return value != MARKER;
		}));

		uweight = 1;
		updater.UpdateLookupOneByOne(&uid, &uweight, 1, lookup.data() + PADDING);

		ASSERT_TRUE(std::none_of(lookup.begin(), lookup.begin() + PADDING, [](auto value) {
			return value != MARKER;
		}));

		ASSERT_TRUE(std::none_of(lookup.begin() + PADDING, lookup.end() - PADDING, [&](auto value) {
			return std::find(ids.begin(), ids.end(), value) == ids.end();
		}));

		dist.clear();
		for (auto i = lookup.cbegin() + PADDING, end = lookup.cend() - PADDING; i != end; ++i)
		{
			++dist[*i];
		}

		ASSERT_TRUE(std::none_of(dist.begin(), dist.end(), [&](const auto& value) {
			return value.second < CELLS_PER_WEIGHT_UNIT;
		}));

		ASSERT_TRUE(std::none_of(lookup.end() - PADDING, lookup.end(), [](auto value) {
			return value != MARKER;
		}));
	}
}

struct SmallConfig
{
	using RealId = std::uint32_t;
	using Index = std::uint32_t;
	using UnweightedIndex = std::uint32_t;
	using Weight = std::uint32_t;
	static const Weight MaxWeight = 5;
	static const std::size_t UnweightedMultiplier = 1 << 5;
	static constexpr std::mt19937::result_type RNG_SEED = 42;
	static constexpr std::size_t DEFAULT_UNWEIGHTED_SIZE = 1 << 16;
	static constexpr double DEFAULT_DEVIATION_TOLERANCE = 0.05;
	static constexpr std::size_t PATCH_LIMIT = 16000;
};

using TestWeightUpdater = chash::BasicWeightUpdater<SmallConfig>;
//using TestWeightUpdater = chash::WeightUpdater;

TEST(Basic, BulkUpdate)
{
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

	auto rsize = TestWeightUpdater::LookupRequiredSize(
	        ids.size(), 2);

	auto opt = TestWeightUpdater::MakeWeightUpdater(
	        reals.data(),
	        ids.data(),
	        weights.data(),
	        ids.size(),
	        REAL_MAPPINGS_LIMIT,
	        2,
	        rsize);
	ASSERT_TRUE(opt);

	auto opt2 = TestWeightUpdater::MakeWeightUpdater(
	        reals.data(),
	        ids.data(),
	        weights.data(),
	        ids.size(),
	        REAL_MAPPINGS_LIMIT,
	        2,
	        rsize);
	ASSERT_TRUE(opt);

	std::vector<std::uint32_t> lookup(rsize, MARKER);
	opt.value().InitLookup(lookup.data());

	std::vector<std::uint32_t> lookup2(rsize, MARKER);
	opt2.value().InitLookup(lookup2.data());

	weights[2] = 2;

	opt.value().UpdateLookupOneByOne(ids.data() + 2, weights.data() + 2, 1, lookup.data());
	auto patch = opt2.value().Update(ids.data() + 2, weights.data() + 2, 1);
	opt2.value().Update(lookup2.data(), patch);

	ASSERT_TRUE(std::equal(lookup.begin(), lookup.end(), lookup2.begin()));

	weights[0] = 0;

	opt.value().UpdateLookupOneByOne(ids.data(), weights.data(), 1, lookup.data());
	patch = opt2.value().Update(ids.data(), weights.data(), 1);
	opt2.value().Update(lookup2.data(), patch);

	ASSERT_TRUE(std::equal(lookup.begin(), lookup.end(), lookup2.begin()));

	weights[2] = 0;
	weights[3] = 2;

	opt.value().UpdateLookupOneByOne(ids.data() + 2, weights.data() + 2, 2, lookup.data());
	patch = opt2.value().Update(ids.data() + 2, weights.data() + 2, 2);
	opt2.value().Update(lookup2.data(), patch);

	ASSERT_TRUE(std::equal(lookup.begin(), lookup.end(), lookup2.begin()));

	weights[3] = 0;
	patch = opt2.value().Update(ids.data()+3, weights.data()+3,1);

	ASSERT_EQ(patch.size(), 0);
	ASSERT_TRUE(opt2.value().Disabled());

	weights = {0, 1, 2, 3, 4};

	opt.value().UpdateLookupOneByOne(ids.data(), weights.data(), 5, lookup.data());
	patch = opt2.value().Update(ids.data(), weights.data(), 5);
	opt2.value().Update(lookup2.data(), patch);

	ASSERT_TRUE(std::equal(lookup.begin(), lookup.end(), lookup2.begin()));

}

}