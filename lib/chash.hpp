#pragma once

#include <cassert>
#include <cmath>
#include <random>
#include <unordered_map>
#include <vector>

#include "bit-reverse.hpp"
#include "unweighted.hpp"
#include "utils.hpp"

namespace balancer
{
using Weight = std::uint8_t;
static constexpr auto RNG_SEED = 42;

template<typename Index>
struct RealInfo
{
	std::vector<Index> indices;
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

template<typename Real, std::uint8_t LookupBits>
class Chash
{
public:
	static constexpr auto MaxWeight = 100;

protected:
	using RealId = std::uint16_t;
	using Key = std::uint32_t;
	using Index = Key;
	static constexpr auto lookup_size = 1 << LookupBits;
	static constexpr Index lookup_mask = lookup_size - 1;
	Index segments_per_weight_unit_ = 20;
	/* Real indices are not consistent between different balancers. More over
	 * Real that was was removed from config is not guaranteed to get the same
	 * index.
	 */
	std::unordered_map<RealId, Real> to_real_;
	std::unordered_map<Real, RealId> to_id_;

	std::map<RealId, RealInfo<Index>> info_;
	std::vector<RealId> lookup_;
	std::vector<bool> enabled_;

	/* @brief Returns number of segments as if they were fairly distributed
	 * between active reals
	 */
	std::size_t Fair()
	{
		return lookup_.size() / to_real_.size();
	}

	std::size_t HardClampedCellCount(RealId id, Weight weight)
	{
		return weight * segments_per_weight_unit_;
	}

	void UpdateWeight(RealId id, Weight weight)
	{
		auto& info = info_.at(id);
		info.desired = weight;
		std::size_t segments_target = HardClampedCellCount(id, weight);
		if (segments_target < info.enabled)
		{
			while (info.enabled > segments_target)
			{
				DisableSegment(id);
			}
		}
		else
		{
			while (info.enabled < segments_target)
			{
				EnableSegment(id);
			}
		}
	}

	/* @brief Instead of disabling segments one by one when lookup ring is
	 * created, this function marks marks all the positions being disabled
	 * and then loops throgh lookup ring recoloring according to current state
	 */
	void InitWeights(const std::map<Real, Weight>& reals)
	{
		for (auto& [real, weight] : reals)
		{
			auto& info = info_.at(to_id_.at(real));
			while (info.enabled > HardClampedCellCount(to_id_.at(real), weight))
			{
				--info.enabled;
				enabled_.at(info.indices.at(info.enabled)) = false;
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
	/* @brief Marks the last enabled segment in the chain of segmentsfor \id as
	 * disabled and recolors corresponding lookup position whith id that is to
	 * immediate left. Doesn't take into account if that position is of the same
	 * color. We rely on the fact that such occurances are comparatively rare
	 * and the lower the target weight the rarer they become
	 */
	void DisableSegment(RealId id)
	{
		auto& donor = info_.at(id);
		auto disable = donor.indices.at(--donor.enabled);
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

	void EnableSegment(RealId id)
	{
		auto& receiver = info_.at(id);
		auto enable = receiver.indices[receiver.enabled];
		for (std::size_t i{enable}; !enabled_[i]; NextRingPosition(lookup_size, i))
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

		std::size_t i = 0;
		for (auto& [real, weight] : reals)
		{
			to_id_[real] = i;
			to_real_[i] = real;
			info_[i].desired = weight;
			++i;
		}

		std::mt19937 seq(RNG_SEED);
		std::vector<Real> realv;
		for (auto& r : reals)
		{
			realv.push_back(r.first);
		}

		// Pool with different matchings from real to hash
		std::vector<Unweighted<Real>> unweight;
		for (std::size_t i = 0; i < uwtd_count; ++i)
		{
			auto salt = seq();
			unweight.emplace_back(realv, salt);
		}

		std::cout << "Generated unweighted rings." << std::endl;

		std::size_t u{};
		// Fill lookup ring choosing for every next index middle of yet unfilled range
		auto seql = [&]() {
			auto n = seq();
			return n;
		};
		for (std::uint32_t i = 0, pos = 0; i < lookup_size; ++i, pos = ReverseBits<LookupBits>(i))
		{
			Real r = unweight[u].Match(seql());
			RealId rid = to_id_[r];
			lookup_[pos] = rid;
			std::vector<Index>& indices = info_[rid].indices;
			indices.push_back(pos);
			u = NextRingPosition(unweight.size(), u);
		}

		for (auto& [real, info] : info_)
		{
			info.enabled = info.indices.size();
			info.actual = info.indices.size();
		}

		std::cout << "Colored lookup ring." << std::endl;

		InitWeights(reals);
		std::cout << "Initialized weights." << std::endl;
	}

	Real Lookup(Key idx)
	{
		return to_real_[lookup_[idx & lookup_mask]];
	}
};

} // namespace balancer