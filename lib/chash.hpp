#pragma once

#include <cassert>
#include <cmath>
#include <random>
#include <unordered_map>
#include <vector>

#include "bit-reverse.hpp"
#include "common.hpp"
#include "delta.hpp"
#include "id-manager.hpp"
#include "unweighted.hpp"
#include "utils.hpp"

namespace chash
{

static constexpr auto RNG_SEED = 42;

template<typename Index>
struct RealInfo
{
	std::vector<Index> heads;
	std::size_t enabled;
	std::size_t actual;
	Weight desired;
};

#if TODO
class Weight
{
	static constexpr max()
	{
		return 100;
	}
};
#endif

/* Real has to be comparable (map ordering, collision resolution)*/
template<typename Real, std::uint8_t LookupBits>
class Chash
{
public:
	static constexpr auto MaxWeight = 100;

protected:
	using Key = Index;
	static constexpr auto lookup_size = 1 << LookupBits;
	static constexpr Index lookup_mask = lookup_size - 1;
	Index slices_per_weight_unit_ = 20;
	/* Real indices are not consistent between different balancers. More over
	 * Real that was was removed from config is not guaranteed to get the same
	 * index.
	 */
	IdManager<Real> idm_;

	// Pool with different matchings from real to hash
	std::vector<Unweighted<Real>> unweighted_;

	std::map<RealId, RealInfo<Index>> info_;
	std::vector<RealId> lookup_;
	std::vector<bool> enabled_;

	/* @brief Returns number of slices as if they were fairly distributed
	 * between active reals
	 */
	std::size_t Fair()
	{
		return lookup_.size() / idm_.size();
	}

	/* @brief returns cell count for \id corresponding to \weight. Due to
	 * random nature of cell distribution between reals we cap cell count
	 * for real at a value that is available to every real
	 */
	std::size_t ClampedCellCount(RealId id, Weight weight)
	{
		return weight * slices_per_weight_unit_;
	}

	void Add(const Real& real)
	{
		RealId id = AssignId(real);

		for (auto& ring: unweighted_)
		{
			ring.Add(real);
		}

		for (int i = 0; i < lookup_.size(); ++i)
		{
			auto& ring = unweighted_[RingPosition(unweighted_.size(), i)];


			// TODO
		}
	}

	/* @brief disables/enables \id slices one by one until the /weight requirement
	 * is met
	 */
	std::vector<Slice> UpdateWeight(RealId id, Weight weight)
	{
		auto& info = info_.at(id);
		info.desired = weight;
		std::size_t slices_target = ClampedCellCount(id, weight);

		while (info.enabled > slices_target)
		{
			DisableSlice(id);
		}

		while (info.enabled < slices_target)
		{
			EnableSlice(id);
		}

		// TODO
		return {};
	}

	/* @brief Instead of disabling slices one by one when lookup ring is
	 * created, this function marks marks all the positions being disabled
	 * and then loops throgh lookup ring recoloring according to current state.
	 */
	void InitWeights(const std::map<Real, Weight>& reals)
	{
		for (auto& [real, weight] : reals)
		{
			RealId id = idm_.GetId(real);
			auto& info = info_.at(id);
			while (info.enabled > ClampedCellCount(id, weight))
			{
				--info.enabled;
				enabled_.at(info.heads.at(info.enabled)) = false;
			}
		}

		std::size_t i{};
		while (!enabled_.at(i))
		{
			++i;
		}
		RealId tint = lookup_.at(i);
		for (std::size_t count = lookup_.size(); count != 0; --count, i = NextRingPosition(lookup_size, i))
		{
			if (enabled_.at(i))
			{
				tint = lookup_.at(i);
			}
			else
			{
				lookup_.at(i) = tint;
			}
		}
	};

	std::unordered_map<RealId, Index> debug;
	/* @brief Marks the last enabled cell in the chain of head cells for \id as
	 * disabled and removes slice from lookup starting at the cell position.
	 * this is done by combining it with the slice to immediate left. Doesn't
	 * take into account if that slice is of the same color. We rely on the
	 * fact that such occurances are comparatively rare and the lower the
	 * target weight the rarer they become.
	 */
	void DisableSlice(RealId id)
	{
		auto& donor = info_.at(id);
		auto disable = donor.heads.at(--donor.enabled);
		std::size_t i{disable};
		if (enabled_[disable])
		{
			++debug[id];
			enabled_[disable] = false;
		}
		RealId tint = lookup_[PrevRingPosition(lookup_size, i)];
		auto& receiver = info_.at(tint);
		for (; !enabled_[i]; i = NextRingPosition(lookup_size, i))
		{
			receiver.actual++;
			--donor.actual;
			lookup_[i] = tint;
		}
	}

	/* @brief Marks the cell in chain of head cells for \id directly past the
	 * last enabled as enabled and adds new slice starting at corresponding
	 * position.
	 */
	void EnableSlice(RealId id)
	{
		auto& receiver = info_.at(id);
		auto enable = receiver.heads[receiver.enabled];
		for (std::size_t i{enable}; !enabled_[i]; i = NextRingPosition(lookup_size, i))
		{
			++receiver.actual;
			--info_.at(lookup_[i]).actual;
			lookup_[i] = id;
		}
		++receiver.enabled;
	}

public:
	Chash(const std::map<Real, Weight>& reals, std::size_t uwtd_count) :
	        lookup_(lookup_size, std::numeric_limits<RealId>::max()),
	        enabled_(lookup_size, true)
	{
		assert(lookup_.size() == lookup_size);
		assert(lookup_.size() == enabled_.size());

		for (auto& [real, weight] : reals)
		{
			info_.at(idm_.Assign(real)).desired = weight;
		}

		std::mt19937 seq(RNG_SEED);
		std::vector<Real> realv;
		for (auto& r : reals)
		{
			realv.push_back(r.first);
		}


		for (std::size_t i = 0; i < uwtd_count; ++i)
		{
			auto salt = seq();
			unweighted_.emplace_back(realv, salt);
		}

		std::cout << "Generated unweighted rings." << std::endl;

		std::size_t u{};
		for (std::uint32_t i = 0, pos = 0; i < lookup_size; ++i, pos = ReverseBits<LookupBits>(i))
		{
			Real r = unweighted_[u].Match(i);
			RealId rid = idm_.GetId(r);
			lookup_[pos] = rid;
			std::vector<Index>& indices = info_[rid].heads;
			indices.push_back(pos);
			u = NextRingPosition(unweighted_.size(), u);
		}

		for (auto& [real, info] : info_)
		{
			info.enabled = info.heads.size();
			info.actual = info.heads.size();
		}

		std::cout << "Colored lookup ring." << std::endl;

		InitWeights(reals);
		std::cout << "Initialized weights." << std::endl;
	}

	Delta<Real> UpdateReals(const std::map<Real, Weight>& reals)
	{
		DeltaBuilder<Real> builder;
		for (auto& [real, weight] : reals)
		{
			auto it = info_.find(real);
			if (it == info_.end())
			{
				Add(real);
				builder.Add(real, idm_.GetId(real));
				it = info_.find(real);
			}

			auto slices = UpdateWeight(it->second, weight);
			builder.Add(slices);

			if (weight == 0)
			{
				Remove(real);
				builder.Remove(real);
			}
		}
		return builder.GetDelta();
	}

	/* @brief Truncates \idx to lookup ring size and returns corresponding real*/
	Real Lookup(Key idx)
	{
		return idm_.GetReal(lookup_[idx & lookup_mask]);
	}
};

} // namespace chash