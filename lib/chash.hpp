#pragma once
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
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

template<typename Config>
struct PatchOperation
{
	using Index = typename Config::Index;
	using RealId = typename Config::RealId;
	Index head;
	RealId id;
	bool on;
	PatchOperation(Index h, RealId i, bool o) : head{h}, id{i}, on{o}
	{
	}
};

template<typename Config>
using BasicPatch = std::vector<PatchOperation<Config>>;

template<typename Config>
struct BasicRealInfo
{
	using Index = typename Config::Index;
	using RealId = typename Config::RealId;
	using Weight = typename Config::Weight;
	std::vector<Index> heads;
	Index enabled = 0;
	Index weight = 0;
	Index cells = 0;

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

	void Update(BasicPatch<Config>& patch, RealId id, Weight w, Index segments_per_weight_unit)
	{
		auto l = heads.begin() + enabled;
		const auto next_enabled = w * segments_per_weight_unit;
		auto r = heads.begin() + next_enabled;
		const bool op = next_enabled > enabled;
		if (!op)
		{
			std::swap(l, r);
		}

		for (; l != r; ++l)
		{
			patch.emplace_back(*l, id, op);
		}

		enabled = next_enabled;
		weight = w;
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
	const Index lookup_size_;
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
		auto ts = std::chrono::steady_clock::now();
		std::cout << "ts\n";
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
			if (!info.Disabled())
			{
				++updater.reals_active_;
			}
		}

		auto t1 = std::chrono::steady_clock::now();
		std::cout << "t1\n";

		std::vector<Unweighted<RealId>> unweighted;

		std::mt19937 seq(Config::RNG_SEED);
		for (std::size_t i = 0; i < side_rings_count; ++i)
		{
			auto salt = seq();
			//			unweighted.emplace_back(Unweighted<RealId>::Make(
			//			        cnt * Config::UnweightedMultiplier, reals, ids, cnt, salt));
			unweighted.emplace_back(Unweighted<RealId>::Make(
			        8096, reals, ids, cnt, salt));
		}
		auto t2 = std::chrono::steady_clock::now();
		Index need_heads = LookupRequiredSize(cnt, segments_per_weight);

		std::vector<Index> seq_cache;
		seq_cache.reserve(need_heads);
		for (Index i = 0; i < need_heads; ++i)
		{
			seq_cache.push_back(seq());
		}

		auto t21 = std::chrono::steady_clock::now();

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
		auto t3 = std::chrono::steady_clock::now();

		for (auto& [id, info] : updater.heads_)
		{
			GCC_BUG_UNUSED(id);
			std::for_each(info.cbegin(),
			              info.cend(),
			              [&](const Index p) {
				              updater.enabled_[p] = true;
			              });
		}

		auto te = std::chrono::steady_clock::now();

		std::cout << "TTR: start: "
		          << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - ts).count()
		          << " unweighted: "
		          << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count()
		          << " rng: "
		          << std::chrono::duration_cast<std::chrono::milliseconds>(t21 - t2).count()
		          << " marking: "
		          << std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t21).count()
		          << " headcount: "
		          << std::chrono::duration_cast<std::chrono::milliseconds>(te - t3).count()
		          << "\n";
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
		RealId old = lookup[start];
		if (old == id)
		{
			return 0;
		}
		Index changed{};
		for (Index i{start}; !enabled_[i]; i = NextRingPosition(LookupSize(), i))
		{
			lookup[i] = id;
			++changed;
		}
		heads_[id].cells += changed;
		heads_[old].cells -= changed;
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

		const auto maystart = donor.DisableOne();
		if (!maystart)
		{
			return 0;
		}
		const auto& start = maystart.value();

		if (donor.Disabled())
		{
			--reals_active_;
		}

		enabled_[start] = false;

		if (Disabled())
		{
			std::fill(lookup, lookup + lookup_size_, Invalid());
			return LookupSize();
		}

		Index prev = PrevRingPosition(lookup_size_, start);
		RealId shadow = lookup[prev];

		Index changed = ColorSlice(shadow, start, lookup);
		return changed;
	}

	/* @brief Adds new slice starting at corresponding
	 * position.
	 */
	Index EnableSlice(RealId id, RealId* lookup)
	{
		auto& receiver = heads_.at(id);

		if (receiver.Disabled())
		{
			++reals_active_;
		}

		const auto maystart = receiver.EnableOne();
		if (!maystart)
		{
			return 0;
		}
		const auto& start = maystart.value();

		if (reals_active_ == 1)
		{
			enabled_[start] = true;
			if (*lookup != id)
			{
				std::fill(lookup, lookup + LookupSize(), id);
				receiver.cells = LookupSize();
				return LookupSize();
			}
			return 0;
		}

		Index changed = ColorSlice(id, start, lookup);
		enabled_[start] = true;

		return changed;
	}

	using layout = std::unordered_map<Index, RealId>;
	std::pair<layout, layout> PlanUpdate()
	{
		return {};
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
		if (info.weight == weight)
		{
			return;
		}

		while (info.enabled > weight * segments_per_weight_)
		{
			DisableSlice(id, lookup);
		}

		while (info.enabled < weight * segments_per_weight_)
		{
			EnableSlice(id, lookup);
		}

		total_weight_ -= info.weight;
		total_weight_ += weight;
		info.weight = weight;
	}

	Index ConfiguredCells(Weight weight) const
	{
		return static_cast<std::uint64_t>(lookup_size_) * weight / total_weight_;
	}

	double Deviation(RealInfo& real) const
	{
		return (static_cast<double>(real.cells) - ConfiguredCells(real.weight)) / ConfiguredCells(real.weight);
	}

	void AdjustDown(RealId id, RealInfo& info, RealId* lookup)
	{
		Index target = ConfiguredCells(info.weight);
		Index vicinity = static_cast<Index>(info.weight * segments_per_weight_ * Config::DEFAULT_DEVIATION_TOLERANCE);
		while (!info.Disabled() && (info.cells - target > vicinity))
		{
			DisableSlice(id, lookup);
		}
	}

	void AdjustUp(RealId id, RealInfo& info, RealId* lookup)
	{
		Index target = ConfiguredCells(info.weight);
		Index vicinity = static_cast<Index>(info.weight * segments_per_weight_ * Config::DEFAULT_DEVIATION_TOLERANCE);
		while (!info.Full() && (target - info.cells > vicinity))
		{
			EnableSlice(id, lookup);
		}
	}

	/*
	 * @brief as last step, after consistently building ring data one might
	 * consider sacrifice some consistency in sake of reducing the effective
	 * weight error.
	 */
	void Adjust(RealId* lookup)
	{
		return;
		if (Disabled())
		{
			return;
		}

		std::vector<RealId> order;
		order.reserve(heads_.size());
		for (auto& [id, info] : heads_)
		{
			GCC_BUG_UNUSED(info);
			if (info.weight != 0)
			{
				order.push_back(id);
			}
		}
		std::sort(order.begin(), order.end(), [&](RealId a, RealId b) {
			if (heads_[a].weight > heads_[b].weight)
			{
				return true;
			}
			if (heads_[a].weight < heads_[b].weight)
			{
				return false;
			}
			return Deviation(heads_[a]) > Deviation(heads_[b]);
		});

		for (auto id : order)
		{
			auto& info = heads_[id];

			if (info.weight == 0)
			{
				continue;
			}

			if (auto deviation = Deviation(info); deviation > Config::DEFAULT_DEVIATION_TOLERANCE)
			{
				AdjustDown(id, info, lookup);
			}
			else if (deviation < -Config::DEFAULT_DEVIATION_TOLERANCE)
			{
				AdjustUp(id, info, lookup);
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
					std::for_each(h->second.heads.begin() + updated,
					              h->second.heads.begin() + current,
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

	std::string Report(RealId* lookup)
	{
		std::unordered_map<RealId, Index> dist;
		std::for_each(lookup, lookup + lookup_size_, [&](RealId id) {
			++dist[id];
		});
		std::stringstream ss;
		for (auto& [id, count] : dist)
		{
			ss << "id: " << id << "count: " << count << "\n";
		}
		return ss.str();
	}

	// struct Update
	// {
	// 	enum {
	// 		Disable,
	// 		Enable,
	// 		Update
	// 	} operation;
	// 	struct Info{
	// 		RealId id;
	// 		bool on;
	// 	};
	// 	std::map<Index, Info> segments;
	// };

	void UpdateLookupOneByOne(const RealId* ids, const Weight* weights, Index count, RealId* lookup)
	{
		// auto [on, off] = PlanUpdate();

		for (Index i = 0; i < count; ++i)
		{
			UpdateWeight(ids[i], weights[i], lookup);
		}
	}

	Index UpdateStart()
	{
		for (auto& [id, info] : heads_)
		{
			GCC_BUG_UNUSED(id);
			if (info.enabled != 0)
			{
				return info.heads[0];
			}
		}
		return 0;
	}

	BasicPatch<Config> Update(const RealId* ids, const Weight* weights, Index count)
	{
		BasicPatch<Config> patch;
		patch.reserve(Config::PATCH_LIMIT);
		for (Index i = 0; i < count; ++i)
		{
			const RealId id = ids[i];
			const Weight weight = weights[i];
			if (heads_.find(id) == heads_.end())
			{
				continue;
			}
			auto& info = heads_.at(id);
			if (info.weight == weight)
			{
				continue;
			}

			if (info.weight == 0)
			{
				++reals_active_;
			}

			info.Update(patch, id, weight, segments_per_weight_);

			if (weight == 0)
			{
				--reals_active_;
			}

			if (Disabled())
			{
				for (PatchOperation<Config>& p : patch)
				{
					enabled_[p.head] = p.on;
				}
				patch.clear();
			}
		}
		std::sort(patch.begin(),
		          patch.end(),
		          [](const PatchOperation<Config>& a, const PatchOperation<Config>& b) {
			          return a.head < b.head;
		          });
		for (PatchOperation<Config>& p : patch)
		{
			enabled_[p.head] = p.on;
		}
		return patch;
	}

	void Update(Index* lookup, const BasicPatch<Config>& patch)
	{
		if (patch.size() == 0)
		{
			return;
		}
		Index start = UpdateStart();
		const typename BasicPatch<Config>::const_iterator patch_split =
		        std::lower_bound(patch.begin(), patch.end(), start, [&](const auto& a, Index b) {
			        return a.head < b;
		        });

		auto up = [&](const PatchOperation<Config>& op, Index next) {
			RealId id;
			if (op.on)
			{
				id = op.id;
			}
			else
			{
				id = lookup[PrevRingPosition(lookup_size_, op.head)];
			}

			if (lookup[op.head] == id)
			{
				return;
			}
			lookup[op.head] = id;
			for (Index pos = NextRingPosition(lookup_size_, op.head);
			     !enabled_[pos] && pos != next;
			     pos = NextRingPosition(lookup_size_, pos))
			{
				lookup[pos] = id;
			}
		};

		for (auto op = patch_split; op != patch.cend(); ++op)
		{
			auto next_op = op + 1;
			if (next_op == patch.cend())
			{
				next_op = patch.begin();
			}

			up(*op, next_op->head);
		}

		for (auto op = patch.cbegin(); op != patch_split; ++op)
		{
			auto next_op = op + 1;
			if (next_op == patch.cend())
			{
				next_op = patch.begin();
			}

			up(*op, next_op->head);
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

	/*
	 * @brief Fill segment heads. Find last head and use it's color ass starting.
	 * Color the lookup start to finish, changing the color when encountering
	 * alredy colored cell (segment head).
	 */
	void InitLookup(RealId* lookup)
	{
		std::cout << "Initializing lookup\n";
		std::fill(lookup, lookup + lookup_size_, Invalid());

		if (Disabled())
		{
			return;
		}

		Index last = 0;
		for (auto& [id, info] : heads_)
		{
			std::for_each(info.cbegin(),
			              info.cend(),
			              [&](const Index& pos) {
				              lookup[pos] = id;
				              last = std::max(last, pos);
			              });
		}

		RealId tint = *lookup;
		if (!Valid(tint))
		{
			tint = lookup[last];
		}

		std::for_each(lookup, lookup + lookup_size_, [&](RealId& id) {
			if (Valid(id))
			{
				tint = id;
			}
			id = tint;
			++heads_[id].cells;
		});
	}

	bool Disabled() const
	{
		return reals_active_ == 0;
	}
};

using WeightUpdater = BasicWeightUpdater<DefaultConfig>;
using Patch = BasicPatch<DefaultConfig>;

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