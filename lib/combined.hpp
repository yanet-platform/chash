#pragma once

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

struct RealInfo
{
	std::vector<std::uint16_t> indices;
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
	static constexpr Index lookup_mask = lookup_size;
	/* Real indices are not consistent between different balancers. More over
	 * Real that was was removed from config is not guaranteed to get the same
	 * index.
	 */
	std::unordered_map<RealId, Real> to_real_;
	std::map<RealId, RealInfo> info_;
	std::vector<RealId> lookup_;
	std::vector<bool> enabled_;

	std::size_t Fair()
	{
		return lookup_.size() / to_real_.size();
	}

	void UpdateWeight(RealId id, Weight weight)
	{
		RealInfo& info = info_.at(id);
		std::size_t segments_target = std::min(weight * Fair() / MaxWeight, info.indices.size());
		std::cout << to_real_[id] << ' ' << segments_target << '\n';
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

	RealId DifferentLeft(Index pos)
	{
		RingIterator it{lookup_.size(), pos};
		while (lookup_[it] == lookup_[pos])
		{
			--it;
		}
		return lookup_[it];
	}

	void DisableSegment(RealId id)
	{
		auto& donor = info_.at(id);
		auto disable = donor.indices[--donor.enabled];
		enabled_[disable] = false;
		RingIterator it{lookup_.size(), disable};
		RealId tint = lookup_[it - 1];
		auto& receiver = info_.at(tint);
		for (; !enabled_[it]; ++it)
		{
			receiver.actual++;
			--donor.actual;
			lookup_[it] = tint;
		}
	}

	void EnableSegment(RealId id)
	{
		auto& receiver = info_.at(id);
		auto enable = receiver.indices[receiver.enabled];
		for (RingIterator it{lookup_.size(), enable}; !enabled_[it]; ++it)
		{
			++receiver.actual;
			--info_.at(lookup_[it]).actual;
			lookup_[it] = id;
		}
		++receiver.enabled;
	}

public:
	Combined(const std::map<Real, Weight>& reals, std::size_t uwtd_count) :
	        lookup_(lookup_size, std::numeric_limits<RealId>::max()),
	        enabled_(lookup_size, true)
	{
		lookup_.resize(lookup_size);

		std::unordered_map<Real, RealId> to_index;
		std::size_t i = 0;
		for (auto& [real, weight] : reals)
		{
			to_index[real] = i;
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

		RingIterator ridx{uwtd_count};
		// Fill lookup ring choosing for every next index middle of yet unfilled range
		auto seql = [&]() {
			auto n = seq();
			// std::cout << "seq " << n << '\n';
			return n;
		};
		for (auto pos : BitReverseSequence<LookupBits>{})
		{
			if (pos >= lookup_size)
				continue;
			Real r = unweight[ridx].Match(seql());
			RealId rid = to_index[r];
			lookup_[pos] = rid;
			info_[rid].indices.push_back(pos);
			++ridx;
		}

		std::cout << "info: ";
		for (auto& [real, info] : info_)
		{
			info.enabled = info.indices.size();
			std::cout << real << ", ";
		}
		std::cout << '\n';

		std::cout << "Colored lookup ring." << std::endl;
		for (const auto& [real, weight] : reals)
		{
			UpdateWeight(to_index[real], weight);
		}
		std::cout << "Updated weights." << std::endl;
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
		Index norm = std::numeric_limits<Index>::max();
		for (auto& [_, count] : dist)
		{
			norm = std::min(norm, count);
		}

		std::cout << "Norm: " << norm << "\n";
		std::cout << "fair: " << Fair() << '\n';
		std::cout << "Id     Alloc   Active   Enabled   \%Active  \%Enabled Normalized Target\n";
		for (auto& [id, info] : info_)
		{
			std::cout << std::setw(6) << std::left << to_real_[id] << " "
			          << std::setw(8) << std::left << info_.at(id).indices.size()
			          << std::setw(8) << std::left << dist[id] << " (" << std::setw(7) << std::left << info.enabled << ") "
			          << std::setw(8) << std::left << static_cast<double>(dist[id]) * 100.0 / info.indices.size()
			          << " (" << std::setw(5) << std::left << std::fixed << std::setprecision(1) << static_cast<double>(info.enabled) * 100.0 / info.indices.size() << ")  "
			          << std::setw(5) << std::left << static_cast<double>(dist[id]) / norm
			          << std::setw(5) << std::left << static_cast<int>(info_.at(id).desired)
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
		RingIterator it{lookup_.size(), i};
		RealId tint = lookup_[it];
		Index len = 1;
		for (Index i = 0, e = lookup_.size(); i < e; ++i, ++it)
		{
			if (lookup_[it] == tint)
			{
				++len;
			}
			else
			{
				++r[tint][len];
				tint = lookup_[it];
				len = 1;
			}
		}
		for (RealId i = 0; i < r.size(); ++i)
		{
			std::cout << std::setw(6) << std::left << to_real_[i];
			for (auto& [l, cnt] : r[i])
			{
				std::cout << ' ' << l << 'x' << cnt;
			}
			std::cout << '\n';
		}
	}
};

} // namespace balancer