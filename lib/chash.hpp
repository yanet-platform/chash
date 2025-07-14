#pragma once
#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <map>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "bit-reverse.hpp"
#include "common.hpp"
#include "unweighted.hpp"
#include "utils.hpp"

#include <iostream>
#include <sstream>

namespace chash
{

template<typename Config>
struct BasicRealConfig
{
	typename Config::Real real;
	typename Config::RealId id;
	typename Config::Weight weight;
};

#if MOVED
template<typename Config>
struct BasicPatch
{
	using Index = typename Config::Index;
	using RealId = typename Config::RealId;
	std::vector<std::pair<Index, RealId>> on;
	std::vector<Index> off;
	std::optional<Index> offstart;
};

template<typename Config>
struct BasicRealInfo
{
	using Index = typename Config::Index;
	using RealId = typename Config::RealId;
	using Weight = typename Config::Weight;
	std::vector<Index> heads;
	Index enabled = 0;
	Index weight = 0;

	std::optional<typename Config::Index> EnableOne()
	{
		if (enabled == heads.size())
		{
			return std::nullopt;
		}
		return heads[enabled++];
	}

	std::optional<typename Config::Index> DisableOne()
	{
		if (enabled == 0)
		{
			return std::nullopt;
		}
		return heads[--enabled];
	}

	void Update(BasicPatch<Config>& patch, RealId id, Weight w, Index enabled_request)
	{
		auto l = heads.begin() + enabled;
		const auto next_enabled = std::min<Index>(enabled_request, heads.size());
		auto r = heads.begin() + next_enabled;
		weight = w;
		if (next_enabled < enabled)
		{
			patch.off.insert(patch.off.end(), r, l);
			enabled = next_enabled;
			return;
		}

		for (; l != r; ++l)
		{
			patch.on.emplace_back(*l, id);
		}

		enabled = next_enabled;
	}

	auto cbegin()
	{
		return heads.cbegin();
	}

	auto cend()
	{
		return heads.cbegin() + enabled;
	}

	bool Disabled() { return enabled == 0; }
	bool Full() { return enabled == heads.size(); }
};
#endif

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

public:
	std::unordered_map<RealId, Index> track;

private:
	const Index lookup_size_;
	Index reals_active_ = 0;
	Index total_weight_ = 0;
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

#if MOVED
	template<typename RealIter, typename IdIter, typename WeightIter>
	static std::optional<BasicWeightUpdater> MakeWeightUpdater(
	        IdIter ids_begin,
	        IdIter ids_end,
	        RealIter reals_begin,
	        WeightIter weights_begin,
	        Index side_rings_count,
	        Index segments_per_weight,
	        Index lookup_size)
	{
		// auto ts = std::chrono::steady_clock::now();
		if (ids_begin == ids_end ||
		    side_rings_count + segments_per_weight * Config::MaxWeight == 0 ||
		    side_rings_count < 1 ||
		    lookup_size < std::distance(ids_begin, ids_end) * segments_per_weight * Config::MaxWeight)
		{
			return std::nullopt;
		}
		BasicWeightUpdater updater(segments_per_weight, lookup_size);
		auto wi = weights_begin;
		for (auto idi = ids_begin; idi != ids_end; ++idi, ++wi)
		{
			RealInfo& info = updater.heads_[*idi];
			info.weight = std::min<typename Config::Weight>(*wi, 100);
			info.enabled = (info.weight) * segments_per_weight;
			updater.total_weight_ += info.weight;
			if (!info.Disabled())
			{
				++updater.reals_active_;
			}
		}

		// auto t1 = std::chrono::steady_clock::now();

		std::vector<Unweighted<RealId>> unweighted;

		std::mt19937 seq(Config::RNG_SEED);
		for (std::size_t i = 0; i < side_rings_count; ++i)
		{
			auto salt = seq();
			//			unweighted.emplace_back(Unweighted<RealId>::Make(
			//			        cnt * Config::UnweightedMultiplier, reals, ids, cnt, salt));
			unweighted.emplace_back(Unweighted<RealId>::Make(
			        8096, ids_begin, ids_end, reals_begin, salt));
		}
		// auto t2 = std::chrono::steady_clock::now();
		std::size_t cnt = std::distance(ids_begin, ids_end);
		Index need_heads = LookupRequiredSize(cnt, segments_per_weight);

		std::vector<Index> seq_cache;
		seq_cache.reserve(need_heads);
		for (Index i = 0; i < need_heads; ++i)
		{
			seq_cache.push_back(seq());
		}

		// auto t21 = std::chrono::steady_clock::now();

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
			if (updater.heads_.find(rid) != updater.heads_.end())
			{
				updater.heads_[rid].heads.reserve((Config::MaxWeight + 1) * updater.segments_per_weight_);
			}
			updater.heads_[rid].heads.push_back(pos);
			u = NextRingPosition(unweighted.size(), u);
			++distributed;

			if (distributed % (segments_per_weight * cnt) == 0)
			{
				updater.Rebalance(distributed / updater.heads_.size());
			}
		}
		// auto te = std::chrono::steady_clock::now();

		// std::cout << "TTR: start: "
		//           << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - ts).count()
		//           << " unweighted: "
		//           << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count()
		//           << " rng: "
		//           << std::chrono::duration_cast<std::chrono::milliseconds>(t21 - t2).count()
		//           << " marking: "
		//           << std::chrono::duration_cast<std::chrono::milliseconds>(te - t21).count()
		//           << "\n";
		return updater;
	}
#endif

private:
#if MOVED
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
#endif

public:
#if MOVED
	template<typename IdIter, typename WeightIter>
	BasicPatch<Config> Update(IdIter ids_begin, IdIter ids_end, WeightIter weights_begin)
	{
		BasicPatch<Config> patch;
		auto wi = weights_begin;
		for (auto idi = ids_begin; idi != ids_end; ++idi, ++wi)
		{
			if (heads_.find(*idi) == heads_.end())
			{
				continue;
			}
			auto& info = heads_.at(*idi);
			if (info.weight == *wi)
			{
				continue;
			}
			total_weight_ += *wi;
			total_weight_ -= info.weight;

			if (info.weight == 0)
			{
				++reals_active_;
			}

			info.Update(patch, *idi, *wi, *wi * segments_per_weight_);

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
			for (const auto& [_, info] : heads_)
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

	void FixSeam(RealId* lookup, std::vector<bool>& enabled)
	{
		const RealId old = lookup[0];
		const RealId last = lookup[lookup_size_ - 1];
		if (last != old)
		{
			Index i = 0;
			for (; !enabled[i]; ++i)
			{
				lookup[i] = last;
			}
			track[old] -= i;
			track[last] += i;
		}
	}
	// Doesn't handle ring wrap-around
	void EnableDirty(RealId* lookup, std::vector<bool>& enabled, const Index pos, const RealId id)
	{
		enabled[pos] = true;
		const RealId old = lookup[pos];
		lookup[pos] = id;
		Index i = pos + 1;
		for (; !enabled[i]; ++i)
		{
			lookup[i] = id;
		}

		const Index l = i - pos;
		track[old] -= l;
		track[id] += l;
	}
	// Doesn't handle ring wrap-around
	void DisableDirty(RealId* lookup, std::vector<bool>& enabled, const Index pos, const RealId id)
	{
		const RealId old = lookup[pos];
		enabled[pos] = false;
		const RealId tint = lookup[PrevRingPosition(lookup_size_, pos)];
		Index i = pos;
		for (; !enabled[i]; ++i)
		{
			lookup[i] = tint;
		}

		const Index l = i - pos;
		track[old] -= l;
		track[tint] += l;
	}
#endif

#if MOVED
	void Adjust(RealId* lookup, std::vector<bool>& enabled)
	{
		if (total_weight_ == 0)
		{
			return;
		}

		for (auto& [id, info] : heads_)
		{
			if (info.weight == 0)
			{
				continue;
			}
			const auto tolerance = 0.1;
			const Index target = static_cast<std::size_t>(info.weight) * lookup_size_ / total_weight_;

			const Index& cells = track.at(id);

			const Index low = target * (1.0 - tolerance);
			while (cells < low)
			{
				auto opt = heads_[id].EnableOne();
				if (!opt)
				{
					break;
				}

				EnableDirty(lookup, enabled, opt.value(), id);

				FixSeam(lookup, enabled);
			}
		}

		for (auto& [id, info] : heads_)
		{
			if (info.weight == 0)
			{
				continue;
			}
			const auto tolerance = 0.1;
			const Index target = static_cast<std::size_t>(info.weight) * lookup_size_ / total_weight_;

			const Index& cells = track.at(id);

			const Index high = target * (1.0 + tolerance);
			while (cells > high)
			{
				auto opt = heads_[id].DisableOne();
				if (!opt)
				{
					break;
				}

				DisableDirty(lookup, enabled, opt.value(), id);

				FixSeam(lookup, enabled);
			}
		}
	}
#endif

	static bool Valid(RealId id)
	{
		return id != std::numeric_limits<RealId>::max();
	}

	static constexpr RealId Invalid()
	{
		return std::numeric_limits<RealId>::max();
	}

	/*
	 * @brief Fill segment heads. Find last head and use it's color ass starting.
	 * Color the lookup start to finish, changing the color when encountering
	 * alredy colored cell (segment head).
	 */
	template<typename IdIter, typename HIter>
	void InitLookup(IdIter lookup_begin, HIter enabled_begin)
	{
		*lookup_begin = Invalid();
		track.clear();
		if (Disabled())
		{
			track[Invalid()] = lookup_size_;
			return;
		}
		std::fill(enabled_begin, enabled_begin + lookup_size_, false);

		Index last = 0;
		for (auto& [id, info] : heads_)
		{
			std::for_each(info.cbegin(),
			              info.cbegin() + info.enabled,
			              [&](const Index& pos) {
				              *(lookup_begin + pos) = id;
				              *(enabled_begin + pos) = true;
				              last = std::max(last, pos);
			              });
		}

		RealId tint = *(lookup_begin + last);
		Index prev{};
		for (Index pos = 0; pos < lookup_size_; ++pos)
		{
			if (*(enabled_begin + pos))
			{
				track[tint] += pos - prev;
				prev = pos;
				tint = *(lookup_begin + pos);
			}
			*(lookup_begin + pos) = tint;
		}
		track[tint] += lookup_size_ - prev;
	}

	bool Disabled() const
	{
		return reals_active_ == 0;
	}
};

using WeightUpdater = BasicWeightUpdater<DefaultConfig>;
using Patch = BasicPatch<DefaultConfig>;

template<typename RealIter, typename IdIter, typename WeightIter>
std::optional<WeightUpdater> MakeWeightUpdater(
        IdIter ids_begin,
        IdIter ids_end,
        RealIter reals_begin,
        WeightIter weights_begin,
        WeightUpdater::Index side_rings_count,
        WeightUpdater::Index segments_per_weight)
{
	return WeightUpdater::MakeWeightUpdater(
	        ids_begin,
	        ids_end,
	        reals_begin,
	        weights_begin,
	        side_rings_count,
	        segments_per_weight,
	        WeightUpdater::LookupRequiredSize(std::distance(ids_begin, ids_end), segments_per_weight));
}

template<typename RealIter, typename IdIter, typename WeightIter>
std::optional<WeightUpdater> MakeWeightUpdater(
        IdIter ids_begin,
        IdIter ids_end,
        RealIter reals_begin,
        WeightIter weights_begin,
        WeightUpdater::Index side_rings_count,
        WeightUpdater::Index segments_per_weight,
        WeightUpdater::Index lookup_size)
{
	return WeightUpdater::MakeWeightUpdater(
	        ids_begin,
	        ids_end,
	        reals_begin,
	        weights_begin,
	        side_rings_count,
	        segments_per_weight,
	        lookup_size);
}

} // namespace chash