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
	std::size_t enabled = 0;
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
	std::size_t segments_per_weight_;
	std::map<RealId, RealInfo> heads_;
	std::vector<bool> is_enabled_;
	std::size_t enabled_count_;
	BasicWeightUpdater(std::size_t segments_per_weight) :
	        segments_per_weight_{segments_per_weight}
	{
	}

public:
	std::size_t LookupSize() const
	{
		return heads_.size() * Config::MaxWeight * segments_per_weight_;
	}

	template<typename Real>
	static std::optional<BasicWeightUpdater> MakeWeightUpdater(
	        const std::vector<std::pair<Real, RealId>>& reals_list,
	        std::size_t side_rings_count,
	        std::size_t segments_per_weight)
	{
		const std::size_t segments_per_rid = segments_per_weight * Config::MaxWeight;
		std::unordered_map<RealId, Real> reals;
		for (const auto& [real, id]: reals_list)
		{
			reals.emplace(id, real);
		}

		if (reals.empty() || side_rings_count + segments_per_rid == 0)
		{
			return std::nullopt;
		}
		BasicWeightUpdater updater{segments_per_weight};

		std::mt19937 seq(RNG_SEED);

		std::size_t lookup_size = segments_per_rid * reals.size();
		std::size_t lookups_per_unweighted = lookup_size / side_rings_count;
		if (lookup_size % side_rings_count != 0)
		{
			++lookups_per_unweighted;
		}
		std::uint8_t lookup_bits = PowerOfTwoLowerBound(lookup_size);
		Unweighted<RealId> unweighted;
		for (std::uint32_t i = 0, pos = 0; i < (std::uint32_t{1} << lookup_bits); ++i, pos = ReverseBits(lookup_bits, i))
		{
			if (pos >= lookup_size)
			{
				continue;
			}

			if (i % lookups_per_unweighted == 0 || unweighted.empty())
			{
				unweighted = Unweighted(reals, seq());
			}


			RealId rid = unweighted.Match(i);
			updater.heads_[rid].heads.push_back(pos);

			std::vector<Index>& real_heads = updater.heads_[rid].heads;
			if (real_heads.size() == segments_per_rid)
			{
				reals.erase(rid);
				unweighted.erase(rid);
			}
		}

		return updater;
	}

private:
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
		auto disable = donor.heads.at(--donor.enabled);
		std::size_t i{disable};
		is_enabled_[disable] = false;
		if (enabled_count_ == 1)
		{
			return;
		}
		RealId tint = lookup[PrevRingPosition(LookupSize(), i)];
		for (; !is_enabled_[i] && (lookup[i] != tint); i = NextRingPosition(LookupSize(), i))
		{
			lookup[i] = tint;
		}
	}

	/* @brief Marks the cell in chain of head cells for \id directly past the
	 * last enabled as enabled and adds new slice starting at corresponding
	 * position.
	 */
	void EnableSlice(RealId id, RealId* lookup)
	{
		auto& receiver = heads_.at(id);
		auto enable = receiver.heads[receiver.enabled];
		is_enabled_[enable] = true;
		lookup[enable] = id;
		enable = NextRingPosition(LookupSize(), enable);
		for (std::size_t i{enable}; !is_enabled_[i] && (lookup[i] != id); i = NextRingPosition(LookupSize(), i))
		{
			lookup[i] = id;
		}
		++receiver.enabled;
	}

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
			--enabled_count_;
		}

		while (info.enabled < weight * segments_per_weight_)
		{
			EnableSlice(id, lookup);
			++enabled_count_;
		}
	}

public:
	void UpdateLookup(const std::vector<std::pair<RealId, Weight>>& reals, RealId* lookup)
	{
		for (auto& [id, weight] : reals)
		{
			UpdateWeight(id, weight, lookup);
		}
	}

	void InitLookup(RealId* lookup)
	{
		for (auto& [id, info] : heads_)
		{
			for (const auto& head : info.heads)
			{
				lookup[head] = id;
			}
			info.enabled = info.heads.size();
		}
		is_enabled_ = std::vector<bool>(LookupSize(), true);
		enabled_count_ = is_enabled_.size();
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
std::optional<WeightUpdater> MakeWeightUpdater(const std::vector<std::pair<Real, WeightUpdater::RealId>>& reals,
                                               std::size_t side_rings_count,
                                               std::size_t segments_per_weight)
{
	return WeightUpdater::MakeWeightUpdater(reals, side_rings_count, segments_per_weight);
}

} // namespace chash