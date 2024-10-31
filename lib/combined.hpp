#pragma once

#include <cassert>
#include <cmath>
#include <iomanip>
#include <random>
#include <unordered_map>
#include <vector>

#include "bit-reverse.hpp"
#include "printers.hpp"
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
class Combined
{
public:
	static constexpr auto MaxWeight = 100;

private:
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
	Combined(const std::map<Real, Weight>& reals, std::size_t uwtd_count) :
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

	void Report()
	{
		std::unordered_map<RealId, Index> dist;
		for (auto id : lookup_)
		{
			++dist[id];
		}
		Index norm = std::numeric_limits<Index>::min();
		for (auto& [_, count] : dist)
		{
			norm = std::max(norm, count);
		}

		std::cout << "Max: " << norm << "\n";
		std::cout << "Fair: " << Fair() << '\n';
		std::cout << "Id         Alloc    Active  Enabled   \%Active  \%Enabled Normalized   Target\n";
		for (auto& [id, info] : info_)
		{
			std::cout << std::setw(10) << std::left << to_real_[id] << " "
			          << std::setw(8) << info_.at(id).indices.size()
			          << std::setw(7) << std::right << dist[id]
			          << std::setw(9) << std::right << Parenthesize(info.enabled)
			          << std::setw(10) << std::right << std::fixed << std::setprecision(1) << static_cast<double>(dist[id]) * 100.0 / info.indices.size() << ' '
			          << std::setw(9) << std::right << Parenthesize(std::fixed, std::setprecision(1), static_cast<double>(info.enabled) * 100.0 / info.indices.size()) << " "
			          << std::setw(10) << std::right << static_cast<double>(dist[id]) * 100 / norm;
			std::cout << std::setw(9) << std::right << static_cast<int>(info_.at(id).desired)
			          << "\n";
		}
	}

	void ReportFrag()
	{
		using SegLength = Index;
		std::vector<std::map<SegLength, std::size_t>> r(info_.size());
		Index i = 0;
		for (; !enabled_[i] && i < lookup_.size(); ++i)
			;
		std::size_t cell{};
		RealId tint = lookup_[cell];
		Index len = 1;
		for (Index i = 0; i < lookup_size; ++i, cell = NextRingPosition(lookup_size, cell))
		{
			if (lookup_[cell] == tint)
			{
				++len;
			}
			else
			{
				++r[tint][len];
				tint = lookup_[cell];
				len = 1;
			}
		}
		for (RealId i = 0; i < r.size(); ++i)
		{
			std::cout << std::setw(10) << std::left << to_real_[i];
			for (auto& [l, cnt] : r[i])
			{
				std::stringstream ss;
				ss << l << 'x' << cnt;
				std::cout << std::setw(7) << ss.str();
			}
			std::cout << '\n';
		}
	}

	void ReportSettings()
	{
		std::cout << "Lookup ring size: " << lookup_size << '\n'
				  << "Lookup mask:" << std::hex << lookup_mask << '\n'
		          << "Segments at 100 weight: " << segments_per_weight_unit_ * 100 << '\n';
	}
};

} // namespace balancer