#pragma once
#include <chash/state.hpp>

#include <chash/bit-reverse.hpp>
#include <chash/unweighted.hpp>

namespace chash
{

template<typename RealId>
template<typename RealIter, typename IdIter, typename WeightIter>
std::optional<State<RealId>> State<RealId>::Make(IdIter ids_begin,
                                 IdIter ids_end,
                                 RealIter reals_begin,
                                 WeightIter weights_begin,
                                 Index side_rings_count,
                                 Index lookup_size)
{
	if (ids_begin == ids_end ||
	    side_rings_count + DEFAULT_SEGMENTS_PER_WEIGHT * MAX_WEIGHT == 0 ||
	    side_rings_count < 1 ||
	    lookup_size < std::distance(ids_begin, ids_end) * DEFAULT_SEGMENTS_PER_WEIGHT * MAX_WEIGHT)
	{
		return std::nullopt;
	}
	State updater(lookup_size);
	auto wi = weights_begin;
	for (auto idi = ids_begin; idi != ids_end; ++idi, ++wi)
	{
		Real<RealId>& info = updater.reals_[*idi];
		info.requested = updater.segments_per_weight_ * std::min<Weight>(*wi, MAX_WEIGHT);
		info.enabled = info.requested;
		updater.total_weight_ += info.enabled;
		if (!info.Disabled())
		{
			++updater.reals_active_;
		}
	}

	std::vector<Unweighted<RealId>> unweighted;

	std::mt19937 seq(RNG_SEED);
	for (std::size_t i = 0; i < side_rings_count; ++i)
	{
		auto salt = seq();
		//			unweighted.emplace_back(Unweighted<RealId>::Make(
		//			        cnt * Config::UnweightedMultiplier, reals, ids, cnt, salt));
		unweighted.emplace_back(Unweighted<RealId>::Make(
		        8096, ids_begin, ids_end, reals_begin, salt));
	}

	std::size_t cnt = std::distance(ids_begin, ids_end);
	Index need_heads = LookupRequiredSize(cnt);

	std::vector<Index> seq_cache;
	seq_cache.reserve(need_heads);
	for (Index i = 0; i < need_heads; ++i)
	{
		seq_cache.push_back(seq());
	}

	std::uint8_t lookup_bits = PowerOfTwoLowerBound(lookup_size);
	std::size_t u{};
	Index distributed{};
	for (Index i = 0, pos = 0; distributed < need_heads; ++i, pos = ReverseBits(lookup_bits, i))
	{
		if (pos >= lookup_size)
		{
			continue;
		}

		RealId rid = unweighted[u].Match(seq_cache.at(distributed));
		if (updater.reals_.find(rid) != updater.reals_.end())
		{
			updater.reals_[rid].heads.reserve((MAX_WEIGHT + 1) * updater.segments_per_weight_);
		}
		updater.reals_[rid].heads.push_back(pos);
		u = NextRingPosition(unweighted.size(), u);
		++distributed;

		if (distributed % (updater.segments_per_weight_ * cnt) == 0)
		{
			updater.Rebalance(distributed / updater.reals_.size());
		}
	}

	return updater;
}

template<typename RealId>
void State<RealId>::Rebalance(Index target)
{
	std::vector<RealId> low;
	std::vector<RealId> high;
	for (auto& [id, info] : reals_)
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

		reals_.at(*l).heads.push_back(reals_.at(*h).heads.back());
		reals_.at(*h).heads.pop_back();

		if (reals_.at(*l).heads.size() == target)
		{
			++l;
			if (l == low.end())
			{
				break;
			}
		}
		if (reals_.at(*h).heads.size() == target)
		{
			++h;
			if (h == high.end())
			{
				break;
			}
		}
	}
}

template<typename RealId>
template<typename IdIter, typename WeightIter>
Patch<RealId> State<RealId>::Update(IdIter ids_begin, IdIter ids_end, WeightIter weights_begin)
{
	Patch<RealId> patch;
	auto wi = weights_begin;
	for (auto idi = ids_begin; idi != ids_end; ++idi, ++wi)
	{
		if (reals_.find(*idi) == reals_.end())
		{
			continue;
		}
		auto& info = reals_.at(*idi);
		if (info.requested == *wi)
		{
			continue;
		}
		total_weight_ += *wi * segments_per_weight_;
		total_weight_ -= info.enabled;

		if (info.requested == 0)
		{
			++reals_active_;
		}

		info.Update(patch, *idi, *wi * segments_per_weight_);

		if (*wi == 0)
		{
			--reals_active_;
		}
	}

	if (!patch.on.empty())
	{
		patch.offstart = patch.on.front().first;
	}
	else if (!patch.off.empty() && !Disabled())
	{
		for (const auto& [_, info] : reals_)
		{
			GCC_BUG_UNUSED(_);
			if (info.enabled != 0)
			{
				patch.offstart = info.heads.front();
				break;
			}
		}
	}

	std::sort(patch.off.begin(), patch.off.end());

	return patch;
}

template<typename RealId>
template<typename enable_t, typename disable_t>
void State<RealId>::Adjust(const std::unordered_map<RealId, Index>& stats, enable_t&& enable, disable_t&& disable)
{
	if (total_weight_ == 0)
	{
		return;
	}

	for (auto& [id, info] : reals_)
	{
		if (info.requested == 0)
		{
			continue;
		}
		const auto tolerance = 0.1;
		const Index target = static_cast<std::size_t>(info.requested) * lookup_size_ / total_weight_;

		const Index& cells = stats.at(id);

		const Index low = target * (1.0 - tolerance);
		while (cells < low)
		{
			auto opt = info.EnableOne();
			if (!opt)
			{
				break;
			}

			enable(opt.value(), id);
		}
	}

	for (auto& [id, info] : reals_)
	{
		if (info.requested == 0)
		{
			continue;
		}
		const auto tolerance = 0.1;
		const Index target = static_cast<std::size_t>(info.requested) * lookup_size_ / total_weight_;

		const Index& cells = stats.at(id);

		const Index high = target * (1.0 + tolerance);
		while (cells > high)
		{
			auto opt = reals_[id].DisableOne();
			if (!opt)
			{
				break;
			}

			disable(opt.value());
		}
	}
}

template<typename RealId>
bool State<RealId>::Enabled() const
{
	return !Disabled();
}

template<typename RealId>
bool State<RealId>::Disabled() const
{
	return reals_active_ == 0;
}

} // namespace chash