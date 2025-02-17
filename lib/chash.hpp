#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <random>
#include <unordered_map>
#include <vector>

#include "bit-reverse.hpp"
#include "common.hpp"
#include "unweighted.hpp"
#include "utils.hpp"

namespace chash
{

static constexpr std::mt19937::result_type RNG_SEED = 42;

template<typename Config>
struct BasicRealConfig
{
	typename Config::Real real;
	typename Config::RealId id;
	typename Config::Weight weight;
};

template<typename Config>
struct BasicRealInfo
{
	std::vector<typename Config::Index> heads;
	typename Config::Index enabled = 0;
};

template<typename Config = DefaultConfig>
class BasicWeightUpdater
{
public:
	using Index = typename Config::Index;
	using RealId = typename Config::RealId;
	using Weight = typename Config::Weight;
	using RealInfo = BasicRealInfo<Config>;

private:
	Index segments_per_weight_;
	std::unordered_map<RealId, RealInfo> heads_;
	std::map<Index, RealId> enabled_;
	Index lookup_size_;
	BasicWeightUpdater(Index segments_per_weight, std::size_t lookup_size) :
	        segments_per_weight_{segments_per_weight},
	        lookup_size_(lookup_size)
	{
	}

public:
	Index LookupSize() const
	{
		return lookup_size_;
	}

	static Index LookupRequiredSize(Index real_count, Index segments_per_weight)
	{
		return real_count * Config::MaxWeight * segments_per_weight;
	}

	template<typename Real>
	static std::optional<BasicWeightUpdater> MakeWeightUpdater(
	        const Real* reals,
	        const RealId* ids,
	        const Weight* weights,
	        Index cnt,
	        Index side_rings_count,
	        Index segments_per_weight,
	        Index lookup_size)
	{
		if (cnt == 0 ||
		    side_rings_count + segments_per_weight * Config::MaxWeight == 0 ||
			side_rings_count < 1 ||
		    lookup_size < segments_per_weight * Config::MaxWeight)
		{
			return std::nullopt;
		}
		BasicWeightUpdater updater(segments_per_weight, lookup_size);
		std::vector<Unweighted<RealId>> unweighted;

		std::mt19937 seq(RNG_SEED);
		for (std::size_t i = 0; i < side_rings_count; ++i)
		{
			auto salt = seq();
			unweighted.emplace_back(reals, ids, cnt, salt);
		}

		for (const RealId* pid = ids; pid != ids + cnt; ++pid)
		{
			if (!std::any_of(
			            unweighted.begin(),
			            unweighted.end(),
			            [&](const auto& ring) {
				            return ring.contains(*pid);
			            }))
			{
				return std::nullopt;
			}
		}

		std::uint8_t lookup_bits = PowerOfTwoLowerBound(lookup_size);
		std::size_t u{};
		Index distributed{};
		Index need_heads = LookupRequiredSize(cnt, segments_per_weight);
		for (Index i = 0, pos = 0; distributed < need_heads; ++i, pos = ReverseBits(lookup_bits, i))
		{
			if (pos >= lookup_size)
			{
				continue;
			}

			RealId rid = unweighted[u].Match(seq());
			updater.heads_[rid].heads.push_back(pos);
			u = NextRingPosition(unweighted.size(), u);
			++distributed;

			if (distributed % (segments_per_weight * cnt) == 0)
			{
				updater.Rebalance(distributed / updater.heads_.size());
			}
		}

		for (Index i = 0; i < cnt; ++i)
		{
			RealInfo& info = updater.heads_.at(ids[i]);
			info.enabled = weights[i] * segments_per_weight;
			for (std::size_t j = 0; j < info.enabled; ++j)
			{
				updater.enabled_[info.heads.at(j)] = ids[i];
			}
		}
		return updater;
	}

private:
	void Rebalance(Index target)
	{
		std::vector<RealId> low;
		std::vector<RealId> high;
		for (auto& [id, info] : heads_)
		{
			if (info.heads.size() > target)
			{
				high.emplace_back(id);
			}
			else if (info.heads.size() < target)
			{
				low.emplace_back(id);
			}
		}

		if (low.empty() || high.empty())
		{
			return;
		}

		auto l = low.begin();
		auto h = high.begin();
		while (l != low.end())
		{

			heads_.at(*l).heads.push_back(heads_.at(*h).heads.back());
			heads_.at(*h).heads.pop_back();

			if (heads_.at(*l).heads.size() == target)
			{
				++l;
				if (l == low.end())
				{
					break;
				}
			}
			if (heads_.at(*h).heads.size() == target)
			{
				++h;
				if (h == high.end())
				{
					break;
				}
			}
		}
	}

	/* @brief Marks the last enabled cell in the chain of head cells for \id as
	 * disabled and removes slice from lookup starting at the cell position.
	 * this is done by combining it with the slice to immediate left. Doesn't
	 * take into account if that slice is of the same color. We rely on the
	 * fact that such occurances are comparatively rare and the lower the
	 * target weight the rarer they become.
	 */
	void DisableSlice(RealId id, RealId* lookup)
	{
		auto& donor = heads_.at(id);
		Index disable = donor.heads.at(--donor.enabled);
		RealId shadow = lookup[(disable ? disable : lookup_size_) - 1];
		auto headIt = enabled_.find(disable);

		Index stop;
		if (auto next = std::next(headIt); next != enabled_.end())
		{
			stop = next->first;
		}
		else
		{
			stop = enabled_.begin()->first;
		}

		for (Index i = disable; i != stop; i = NextRingPosition(LookupSize(), i))
		{
			lookup[i] = shadow;
		}

		enabled_.erase(headIt);
	}

	/* @brief Marks the cell in chain of head cells for \id directly past the
	 * last enabled as enabled and adds new slice starting at corresponding
	 * position.
	 */
	void EnableSlice(RealId id, RealId* lookup)
	{
		auto& receiver = heads_.at(id);
		auto enable = receiver.heads[receiver.enabled];
		enabled_[enable] = id;
		auto headIt = enabled_.find(enable);

		Index stop;
		if (auto next = std::next(headIt); next != enabled_.end())
		{
			stop = next->first;
		}
		else
		{
			stop = enabled_.begin()->first;
		}

		for (std::size_t i{enable}; i != stop; i = NextRingPosition(LookupSize(), i))
		{
			lookup[i] = id;
		}

		++receiver.enabled;
	}

	RealId LookupBeginTint()
	{
		if (auto it = enabled_.begin(); it->first == 0)
		{
			return it->second;
		}
		else
		{
			return std::prev(enabled_.end())->second;
		}
	}

public:
	/* @brief disables/enables \id slices one by one until the /weight requirement
	 * is met
	 */
	void UpdateWeight(RealId id, Weight weight, RealId* lookup)
	{
		if (heads_.find(id) == heads_.end())
		{
			return;
		}
		auto& info = heads_.at(id);

		while (info.enabled > weight * segments_per_weight_)
		{
			DisableSlice(id, lookup);
		}

		while (info.enabled < weight * segments_per_weight_)
		{
			EnableSlice(id, lookup);
		}
	}

	void UpdateLookup(const RealId* ids, const Weight* weights, Index count, RealId* lookup)
	{
		for (Index i = 0; i < count; ++i)
		{
			UpdateWeight(ids[i], weights[i], lookup);
		}
	}

	void InitLookup(RealId* lookup)
	{
		if (enabled_.empty())
		{
			return;
		}

		Index pos = 0;
		RealId tint = LookupBeginTint();

		for (auto& [change, id] : enabled_)
		{
			for (; pos != change; ++pos)
			{
				lookup[pos] = tint;
			}
			tint = id;
		}

		for (; pos != lookup_size_; ++pos)
		{
			lookup[pos] = tint;
		}
	}

	void InitLookup(const std::vector<std::pair<RealId, Weight>>& reals, RealId* lookup)
	{
		InitLookup(lookup);
		if (std::find_if_not(reals.cbegin(), reals.cend(), [](const auto& r) {
			    return r.second == 0;
		    }) != reals.cend())
		{
			UpdateLookup(reals, lookup);
		}
	}
};

using WeightUpdater = BasicWeightUpdater<DefaultConfig>;

template<typename Real>
std::optional<WeightUpdater> MakeWeightUpdater(
        const Real* reals,
        const WeightUpdater::RealId* ids,
        const WeightUpdater::Weight* weights,
        WeightUpdater::Index cnt,
        WeightUpdater::Index side_rings_count,
        WeightUpdater::Index segments_per_weight)
{
	return WeightUpdater::MakeWeightUpdater(
	        reals,
	        ids,
	        weights,
	        cnt,
	        side_rings_count,
	        segments_per_weight,
	        WeightUpdater::LookupRequiredSize(cnt, segments_per_weight));
}

template<typename Real>
std::optional<WeightUpdater> MakeWeightUpdater(
        const Real* reals,
        const WeightUpdater::RealId* ids,
        const WeightUpdater::Weight* weights,
        WeightUpdater::Index cnt,
        WeightUpdater::Index side_rings_count,
        WeightUpdater::Index segments_per_weight,
        WeightUpdater::Index lookup_size)
{
	return WeightUpdater::MakeWeightUpdater(
	        reals,
	        ids,
	        weights,
	        cnt,
	        side_rings_count,
	        segments_per_weight,
	        lookup_size);
}

} // namespace chash