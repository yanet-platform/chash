#include <gtest/gtest.h>

#include "common.h"
#include "../chash.hpp"

namespace
{
using namespace test;

TEST(Adjust, one4all)
{
	UpdaterInput input{.weights = {100, 1, 1, 1}};
	auto opt = MakeUpdater(input);
	auto& u = opt.value();

	std::vector<RealId> lookup(input.lookup_size, 42);
	u.InitLookup(lookup.data());

	std::map<RealId, std::size_t> dist;
	for (auto e : lookup)
	{
		++dist[e];
	}
	ASSERT_EQ(dist.size(), input.ids.size());
	for (auto id : input.ids)
	{
		ASSERT_NE(dist.find(id), dist.end()) << "Does not contain " << id;
	}

	u.Adjust(lookup.data());

	double margin = 0.2;
	std::size_t total = input.TotalWeight();
	for (std::size_t i = 0; i < input.ids.size(); ++i)
	{
		RealId id = input.ids[i];
		Weight w = input.weights[i];
		double requested = double(w) / total;
		double effective = double(dist[id]) / input.lookup_size;
		double deviation = (effective - requested) / requested;
		ASSERT_LT(std::abs(deviation), margin) << "id: " << id;
	}
}

}