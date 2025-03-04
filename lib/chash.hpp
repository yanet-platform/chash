#pragma once
#include <algorithm>
#include <cassert>
#include <cmath>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "bit-reverse.hpp"
#include "common.hpp"
#include "unweighted.hpp"
#include "utils.hpp"

namespace chash
{

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
	typename Config::Index weight = 0;
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
	std::vector<bool> enabled_;
	Index lookup_size_;
	Index reals_active_ = 0;
	Index total_weight_ = 0;
	BasicWeightUpdater(Index segments_per_weight, std::size_t lookup_size) :
	        segments_per_weight_{segments_per_weight},
	        enabled_(lookup_size, false),
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
		for (Index i = 0; i < cnt; ++i)
		{
			RealInfo& info = updater.heads_[ids[i]];
			info.enabled = weights[i] * segments_per_weight;
			info.weight = weights[i];
			updater.total_weight_ += weights[i];
			if (info.enabled != 0)
			{
				++updater.reals_active_;
			}
		}

		std::vector<Unweighted<RealId>> unweighted;

		std::mt19937 seq(Config::RNG_SEED);
		std::set<RealId> unseen{ids, ids + cnt};
		std::set<RealId> remain;
		for (std::size_t i = 0; i < side_rings_count; ++i)
		{
			auto salt = seq();
			auto [ring, contain] = Unweighted<RealId>::Make(reals, ids, cnt, salt, Config::DEFAULT_UNWEIGHTED_SIZE);
			unweighted.emplace_back(std::move(ring));
			remain.clear();
			for (auto id : unseen)
			{
				if (contain.find(id) == contain.end())
				{
					remain.insert(id);
				}
			}
			unseen = std::move(remain);
		}

		if (!unseen.empty())
		{
			// unweighted rings don't contain some reals due to collisions
			return std::nullopt;
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
			updater.enabled_[pos] = true;
			++distributed;

			if (distributed % (segments_per_weight * cnt) == 0)
			{
				updater.Rebalance(distributed / updater.heads_.size());
			}
		}

		for (auto& [id, info] : updater.heads_)
		{
			GCC_BUG_UNUSED(id);
			std::for_each(info.heads.begin() + info.enabled,
			              info.heads.end(),
			              [&](const Index pos) {
				              updater.enabled_[pos] = false;
			              });
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

	Index ColorSlice(RealId id, Index start, RealId* lookup)
	{
		RealId tint = lookup[start];
		if (tint == id)
		{
			return 0;
		}
		Index changed{};
		for (std::size_t i{start}; lookup[i] == tint && !enabled_[i]; i = NextRingPosition(LookupSize(), i))
		{
			lookup[i] = id;
			++changed;
		}
		return changed;
	}

	/* @brief Marks the last enabled cell in the chain of head cells for \id as
	 * disabled and removes slice from lookup starting at the cell position.
	 * this is done by combining it with the slice to immediate left. Doesn't
	 * take into account if that slice is of the same color. We rely on the
	 * fact that such occurances are comparatively rare and the lower the
	 * target weight the rarer they become.
	 */
	Index DisableSlice(RealId id, RealId* lookup)
	{
		auto& donor = heads_.at(id);
		--donor.enabled;

		Index disable = donor.heads.at(donor.enabled);
		RealId shadow = lookup[PrevRingPosition(lookup_size_, disable)];

		enabled_[disable] = false;
		Index changed = ColorSlice(shadow, disable, lookup);
		return changed;
	}

	/* @brief Marks the cell in chain of head cells for \id directly past the
	 * last enabled as enabled and adds new slice starting at corresponding
	 * position.
	 */
	Index EnableSlice(RealId id, RealId* lookup)
	{
		auto& receiver = heads_.at(id);
		if (receiver.enabled == receiver.heads.size())
		{
			return 0;
		}

		if (Disabled())
		{
			std::fill(lookup, lookup + lookup_size_, id);
			enabled_[receiver.heads[0]] = true;
			++receiver.enabled;
			return lookup_size_;
		}

		if ((reals_active_ == 1) && (*lookup == id))
		{
			++receiver.enabled;
			return 0;
		}

		Index start = receiver.heads[receiver.enabled];
		Index changed = ColorSlice(id, start, lookup);
		enabled_[start] = true;

		++receiver.enabled;
		return changed;
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

		Index was = info.enabled;

		while (info.enabled > weight * segments_per_weight_)
		{
			DisableSlice(id, lookup);
		}

		while (info.enabled < weight * segments_per_weight_)
		{
			EnableSlice(id, lookup);
		}

		if (was == 0 && weight != 0)
		{
			++reals_active_;
		}

		if (weight == 0 && was != 0)
		{
			--reals_active_;
			if (reals_active_ == 0)
			{
				std::fill(lookup, lookup + lookup_size_, Invalid());
			}
		}

		total_weight_ -= info.weight;
		total_weight_ += weight;
		info.weight = weight;
	}

	Index ConfiguredCells(Weight weight) const
	{
		return static_cast<std::uint64_t>(lookup_size_) * weight / total_weight_;
	}

	double Deviation(Weight weight, Index effective_cells) const
	{
		return (static_cast<double>(effective_cells) - ConfiguredCells(weight)) / ConfiguredCells(weight);
	}

	void AdjustDown(RealId id, RealInfo& info, RealId* lookup, Index effective_cells)
	{
		Index target = ConfiguredCells(info.weight);
		while ((info.enabled > 1) && (effective_cells > target))
		{
			effective_cells -= DisableSlice(id, lookup);
		}
	}

	void AdjustUp(RealId id, RealInfo& info, RealId* lookup, Index effective_cells)
	{
		Index target = ConfiguredCells(info.weight);
		while ((info.enabled < info.heads.size()) && (effective_cells < target))
		{
			effective_cells += EnableSlice(id, lookup);
		}
		if ((effective_cells > target) && (info.enabled > 0))
		{
			DisableSlice(id, lookup);
		}
	}

	/*
	 * @brief as last step, after consistently building ring data one might
	 * consider sacrifice some consistency in sake of reducing the effective
	 * weight error.
	 */
	void Adjust(RealId* lookup)
	{
		if (Disabled())
		{
			return;
		}

		std::unordered_map<RealId, Index> distribution;
		std::for_each(lookup,
		              lookup + lookup_size_,
		              [&](RealId id) {
			              ++distribution[id];
		              });

		for (auto& [id, info] : heads_)
		{
			if (info.weight == 0)
			{
				continue;
			}

			if (auto deviation = Deviation(info.weight, distribution.at(id)); deviation > Config::DEFAULT_DEVIATION_TOLERANCE)
			{
				AdjustDown(id, info, lookup, distribution.at(id));
			}
			else if (deviation < -Config::DEFAULT_DEVIATION_TOLERANCE)
			{
				AdjustUp(id, info, lookup, distribution.at(id));
			}
		}
	}

	void SetWeights(const RealId* ids, const Weight* weights, Index count)
	{
		for (Index i = 0; i < count; ++i)
		{
			if (auto h = heads_.find(ids[i]); h != heads_.end())
			{
				if (h->second.enabled == 0 && weights[i] != 0)
				{
					++reals_active_;
				}
				if (weights[i] == 0 && h->second.enabled != 0)
				{
					--reals_active_;
				}
				Index& current = h->second.enabled;
				Index updated = weights[i] * segments_per_weight_;
				if (updated > current)
				{
					std::for_each(h->second.heads.begin() + current,
					              h->second.heads.begin() + updated,
					              [&](Index pos) {
						              enabled_[pos] = true;
					              });
				}
				else
				{
					std::for_each(h->second.heads.begin() + current,
					              h->second.heads.begin() + updated,
					              [&](Index pos) {
						              enabled_[pos] = false;
					              });
				}
				current = updated;
				total_weight_ += weights[i];
				total_weight_ -= h->second.weight;
				h->second.weight = weights[i];
			}
		}
	}

	void UpdateLookup(const RealId* ids, const Weight* weights, Index count, RealId* lookup)
	{
		for (Index i = 0; i < count; ++i)
		{
			UpdateWeight(ids[i], weights[i], lookup);
		}
	}

	static bool Valid(RealId id)
	{
		return id != std::numeric_limits<RealId>::max();
	}

	static RealId Invalid()
	{
		return std::numeric_limits<RealId>::max();
	}

	void InitLookup(RealId* lookup)
	{
		std::fill(lookup, lookup + lookup_size_, Invalid());

		if (Disabled())
		{
			return;
		}

		for (auto& [id, info] : heads_)
		{
			std::for_each(info.heads.begin(),
			              info.heads.begin() + info.enabled,
			              [&](const Index& pos) {
				              lookup[pos] = id;
			              });
		}

		RealId tint = *lookup;
		if (!Valid(tint))
		{
			tint = *std::find_if(std::reverse_iterator(lookup + lookup_size_ - 1),
			                     std::reverse_iterator(lookup - 1),
			                     [](const RealId& id) {
				                     return Valid(id);
			                     });
		}

		std::for_each(lookup, lookup + lookup_size_, [&](RealId& id) {
			if (Valid(id))
			{
				tint = id;
			}
			id = tint;
		});
	}

	bool Disabled() const
	{
		return std::find_if(heads_.begin(),
		                    heads_.end(),
		                    [](const auto& e) {
			                    return e.second.enabled > 0;
		                    }) == heads_.end();
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