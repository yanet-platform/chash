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
	static constexpr auto lookup_size = 1 << (LookupBits - 1);
	static constexpr Key lookup_mask = lookup_size;
	/* Real indices are not consistent between different balancers. More over
	 * Real that was was removed from config is not guaranteed to get the same
	 * index.
	 */
	std::unordered_map<RealId, Real> to_real_;
	std::map<RealId, RealInfo> info_;
	std::vector<RealId> lookup_;
	std::vector<bool> enabled_;

	void UpdateWeight(RealId id, Weight weight)
	{
		RealInfo& info = info_.at(id);
		std::size_t segments_target = weight * info.indices.size() / MaxWeight;
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
	        enabled_(lookup_size, true)
	{
		lookup_.resize(lookup_size);

		std::unordered_map<Real, RealId> to_index;
		std::size_t i = 0;
		for (auto& [real, _] : reals)
		{
			(void)_;
			to_index[real] = i;
			to_real_[i] = real;
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
		std::unordered_map<RealId, Key> dist;
		for (auto id : lookup_)
		{
			++dist[id];
		}
		Key norm = std::numeric_limits<Key>::max();
		for (auto& [_, count] : dist)
		{
			norm = std::min(norm, count);
		}
		std::cout << "Norm: " << norm << "\n";
		std::int64_t fair = lookup_.size() / info_.size();
		std::cout << "fair: " << fair << '\n';
		std::cout << "Id     Alloc   Active   Enabled   \%Active  \%Enabled Normalized\n";
		for (auto& [id, info] : info_)
		{
			std::cout << std::setw(6) << std::left << to_real_[id] << " "
			          << std::setw(8) << std::left << info_.at(id).indices.size()
			          << std::setw(8) << std::left << dist[id] << " (" << std::setw(7) << std::left << info.enabled << ") "
			          << std::setw(8) << std::left << static_cast<double>(dist[id]) * 100.0 / info.indices.size()
			          << " (" << std::setw(5) << std::left << std::fixed << std::setprecision(1) << static_cast<double>(info.enabled) * 100.0 / info.indices.size() << ")  "
			          << std::setw(5) << std::left << static_cast<double>(dist[id]) / norm << "\n";
		}
	}
};

} // namespace balancer