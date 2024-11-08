#pragma once

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

static constexpr auto RNG_SEED = 42;

struct RealConfig
{
	RealId id;
	Weight weight;
};

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
	Index full_load_ = slices_per_weight_unit_ * MaxWeight;

	std::unordered_map<RealId, Real> to_real_;
	std::unordered_map<Real, RealId> to_id_;

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
		return lookup_.size() / to_id_.size();
	}

	/* @brief disables/enables \id slices one by one until the /weight requirement
	 * is met
	 */
	std::vector<Slice> UpdateWeight(RealId id, Weight weight)
	{
		auto& info = info_.at(id);
		info.desired = weight * slices_per_weight_unit_;

		while (info.enabled > info.desired)
		{
			DisableSlice(id);
		}

		while (info.enabled < info.desired)
		{
			EnableSlice(id);
		}
	}

	/* @brief Colors segments according to enabled heads colors. Expects at least
	 * single Segment head to be enabled.
	 */
	void FillGaps(const std::map<Real, RealConfig>& reals)
	{
		auto first = std::find(enabled_.begin(), enabled_.end(), true);
		Index start = ats::distance(enabled_.begin(), first);
		RealId tint = lookup_[i];
		for (std::size_t i = 0, pos = 0;
		     i < lookup_.size();
		     ++i, pos = NextRingPosition(lookup_.size(), i))
		{
			if (enabled_[pos])
			{
				tint = lookup_[pos];
			}
			else
			{
				lookup_[pos] = tint;
			}
		}
	};

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
	Chash(const std::map<Real, RealConfig>& reals, std::size_t uwtd_count) :
	        lookup_(lookup_size, std::numeric_limits<RealId>::max()),
	        enabled_(lookup_size, false)
	{
		assert(lookup_.size() == lookup_size);
		assert(lookup_.size() == enabled_.size());

		for (auto& [real, cfg] : reals)
		{
			info_[cfg.id].desired = cfg.weight * slices_per_weight_unit_;
		}

		std::mt19937 seq(RNG_SEED);
		std::vector<Real> realv;
		for (auto& [real, cfg] : reals)
		{
			realv.push_back(real);
			to_id_[real] = cfg.id;
			to_real_[cfg.id] = real;
		}

		for (std::size_t i = 0; i < uwtd_count; ++i)
		{
			auto salt = seq();
			unweighted_.emplace_back(realv, salt);
		}

		std::cout << "Generated unweighted rings." << std::endl;

		RealId current{};
		std::size_t u{};
		for (std::uint32_t i = 0, pos = 0; i < lookup_size; ++i, pos = ReverseBits<LookupBits>(i))
		{
			Real r = unweighted_[u].Match(i);
			RealId rid = to_id_.at(r);
			std::vector<Index>& indices = info_[rid].heads;
			if (indices.size() < full_load_)
			{
				indices.push_back(pos);
				if (indices.size() < info_[rid].desired)
				{
					enabled_[pos] = true;
					lookup_[pos] = rid;
				}
			}
			u = NextRingPosition(unweighted_.size(), u);
		}

		for (auto& [real, info] : info_)
		{
			info.enabled = info.heads.size();
			info.actual = info.heads.size();
		}

		std::cout << "Colored lookup ring." << std::endl;
	}

	bool UpdateWeights(std::vector<std::pair<RealId, RealConfig>>& request)
	{
		for (auto& [id, cfg]: request)
		{
			UpdateWeight(id, cfg);
		}
	}

	/* @brief Truncates \idx to lookup ring size and returns corresponding real*/
	Real Lookup(Key idx)
	{
		return to_real_.at(lookup_[idx & lookup_mask]);
	}
};

} // namespace chash