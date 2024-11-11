#pragma once

#include <cassert>
#include <cmath>
#include <memory_resource>
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
	std::size_t desired;
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
template<typename Real>
class Chash
{
public:
	static constexpr auto MaxWeight = 100;

protected:
	using Key = Index;
	const std::uint8_t lookup_bits_ = 16;
	const Index lookup_size = 1 << lookup_bits_;
	const Index lookup_mask = lookup_size - 1;
	Index slices_per_weight_unit_ = 20;
	Index full_load_ = slices_per_weight_unit_ * MaxWeight;

	std::pmr::unordered_map<RealId, Real> to_real_;
	std::pmr::unordered_map<Real, RealId> to_id_;

	// Pool with different matchings from real to hash
	std::pmr::vector<Unweighted<Real>> unweighted_;

	std::pmr::map<RealId, RealInfo<Index>> info_;
	std::pmr::vector<RealId> lookup_;
	std::pmr::vector<bool> enabled_;

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
	void UpdateWeight(RealId id, Weight weight)
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
	void FillGaps()
	{
		auto first = std::find(enabled_.begin(), enabled_.end(), true);
		Index start = std::distance(enabled_.begin(), first);
		RealId tint = lookup_[start];
		for (std::size_t i = 0, pos = start;
		     i < lookup_.size();
		     ++i, pos = NextRingPosition(lookup_.size(), pos))
		{
			if (enabled_[pos])
			{
				tint = lookup_[pos];
			}
			else
			{
				lookup_[pos] = tint;
				info_[tint].actual++;
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
		enabled_[disable] = false;
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
	Chash(const std::map<Real, RealConfig>& reals,
	      std::size_t uwtd_count,
	      std::uint8_t lookup_bits,
	      std::pmr::memory_resource* mem = std::pmr::get_default_resource()) :
	        lookup_bits_{lookup_bits},
	        to_real_{mem},
	        to_id_{mem},
	        unweighted_{mem},
	        lookup_(lookup_size, std::numeric_limits<RealId>::max(), mem),
	        enabled_(lookup_size, false, mem)
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

		std::size_t u{};
		for (std::uint32_t i = 0, pos = 0; i < lookup_size; ++i, pos = ReverseBits(lookup_bits_, i))
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
					info_[rid].actual++;
				}
			}
			u = NextRingPosition(unweighted_.size(), u);
		}

		for (auto& [real, info] : info_)
		{
			info.enabled = info.desired;
		}

		FillGaps();

		std::cout << "Colored lookup ring." << std::endl;
	}

	void UpdateWeights(std::vector<std::pair<RealId, RealConfig>>& request)
	{
		for (auto& [id, cfg] : request)
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