#include <gtest/gtest.h>

#include "common.h"

#include "../chash.hpp"

namespace
{

using namespace test;

TEST(Balancer, Tight)
{
	UpdaterInput input{};
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
}

TEST(Balancer, Sparse)
{
	UpdaterInput input{};
	input.lookup_size *= 3;
	input.weights = std::vector<Weight>(4, 100);
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
		ASSERT_LT(std::abs(deviation), margin) << "id: " << id;
	}
}

TEST(Balancer, fair10)
{
	UpdaterInput input{.weights = {10, 10, 10, 10}};
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
		ASSERT_NE(dist.find(id), dist.end());
	}

	const double margin = 0.02;
	const std::size_t total = input.TotalWeight();
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

TEST(Balancer, all4one)
{
	UpdaterInput input{.weights = {1, 100, 100, 100}};
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
		ASSERT_NE(dist.find(id), dist.end());
	}

	const double margin = 0.3;
	const std::size_t total = input.TotalWeight();
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

TEST(Balancer, one4all)
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

	double margin = 0.78;
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

TEST(Balancer, one4ten)
{
	UpdaterInput input{.weights = {100, 10, 10, 10}};
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
		ASSERT_NE(dist.find(id), dist.end());
	}

	const double margin = 0.78;
	const std::size_t total = input.TotalWeight();
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

TEST(Balancer, twenty4ten)
{
	UpdaterInput input{.weights = {20, 10, 10, 10}};
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
		ASSERT_NE(dist.find(id), dist.end());
	}

	const double margin = 0.78;
	const std::size_t total = input.TotalWeight();
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

TEST(Balancer, forty4ten)
{
	UpdaterInput input{.weights = {40, 10, 10, 10},
	                   .cells = 40};
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
		ASSERT_NE(dist.find(id), dist.end());
	}

	const double margin = 0.78;
	const std::size_t total = input.TotalWeight();
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

TEST(Balancer, forty4ten1)
{
	UpdaterInput input{.weights = {40, 10, 10, 10},
	                   .cells = 20};
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
		ASSERT_NE(dist.find(id), dist.end());
	}

	const double margin = 0.78;
	const std::size_t total = input.TotalWeight();
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

TEST(Balancer, onedown)
{
	UpdaterInput input{.weights = {2, 1, 1, 0},
	                   .cells = 20};
	auto opt = MakeUpdater(input);
	auto& u = opt.value();

	std::vector<RealId> lookup(input.lookup_size, 42);
	u.InitLookup(lookup.data());

	std::map<RealId, std::size_t> dist;
	for (auto e : lookup)
	{
		++dist[e];
	}
	std::vector<RealId> active;
	for (std::size_t i = 0; i < input.ids.size(); ++i)
	{
		if (input.weights[i] != 0)
		{
			active.push_back(input.ids[i]);
		}
	}
	ASSERT_EQ(dist.size(), active.size());

	for (auto id : active)
	{
		ASSERT_NE(dist.find(id), dist.end());
	}

	const double margin = 0.78;
	const std::size_t total = input.TotalWeight();
	for (std::size_t i = 0; i < input.ids.size(); ++i)
	{
		RealId id = input.ids[i];
		Weight w = input.weights[i];
		if (w == 0)
		{
			continue;
		}
		double requested = double(w) / total;
		double effective = double(dist[id]) / input.lookup_size;
		double deviation = (effective - requested) / requested;
		ASSERT_LT(std::abs(deviation), margin) << "id: " << id;
	}
}

}