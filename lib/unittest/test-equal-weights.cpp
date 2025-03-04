#include <gtest/gtest.h>

#include "common.h"

#include "../chash.hpp"

namespace
{

using namespace test;

TEST(EqualWeights, SparseIterate)
{
	UpdaterInput input{};
	input.lookup_size *= 3;
	for (std::size_t w = 1; w <= 100; w += (w < 10) ? 1 : 10)
	{
		input.weights = std::vector<Weight>(4, w);
		auto opt = MakeUpdater(input);
		ASSERT_TRUE(opt);
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
			ASSERT_NE(dist.find(id), dist.end());
		}

		const double margin = 0.10;
		const std::size_t total = input.TotalWeight();
		for (std::size_t i = 0; i < input.ids.size(); ++i)
		{
			RealId id = input.ids[i];
			Weight w = input.weights[i];
			double requested = double(w) / total;
			double effective = double(dist[id]) / input.lookup_size;
			double deviation = std::abs((requested - effective) / requested);
			if (deviation >= margin)
			{
				std::cout << w << ' ' << deviation << '\n';
			}
			ASSERT_LT(deviation, margin) << "id:" << id;
		}
	}
}

}