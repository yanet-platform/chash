#pragma once

#include <cmath>
#include <iomanip>
#include <random>
#include <unordered_map>
#include <vector>

#include "unweighted.h"

namespace balancer
{

using Weight = std::uint8_t;
static constexpr auto RNG_SEED = 42;

class RingIterator
{
	std::size_t index_ = 0;
	std::size_t size_ = 1;

	void Next()
	{
		if (index_ == size_ - 1)
		{
			index_ = 0;
		}
		else
		{
			++index_;
		}
	}

	void Prev()
	{
		if (index_ == 0)
		{
			index_ = size_ - 1;
		}
		else
		{
			--index_;
		}
	}

public:
	RingIterator(std::size_t size, std::size_t index = 0) :
	        index_{index},
	        size_{size}
	{
	}

	operator std::size_t()
	{
		return index_;
	}

	RingIterator operator++(int)
	{
		RingIterator result{*this};
		Next();
		return result;
	}

	RingIterator& operator++()
	{
		Next();
		return *this;
	}

	RingIterator operator--(int)
	{
		RingIterator result{*this};
		Prev();
		return *this;
	}

	RingIterator& operator--()
	{
		Prev();
		return *this;
	}
};

template<typename Real>
class Combined
{
	using Index = std::uint16_t;
	std::size_t size_;
	/* Real indices are not consistent between different balancers. More over
	 * Real that was was removed from config is not guaranteed to get the same
	 * index.
	 */
	std::unordered_map<Index, Real> to_real_;
	std::vector<Index> lookup_;

public:
	Combined(const std::map<Real, Weight>& reals, std::size_t uwtd_count, std::size_t segments) :
	        size_{segments * reals.size()}
	{
		lookup_.resize(size_);
		std::unordered_map<Real, Index> to_index;
		std::size_t i = 0;
		for (auto& [real, weight] : reals)
		{
			(void)weight;
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
		std::vector<Unweighted<Real>> unweight;
		for (std::size_t i = 0; i < uwtd_count; ++i)
		{
			unweight.emplace_back(realv, seq());
		}

		std::cout << "Generated unweighted rings." << std::endl;

		RingIterator ridx{uwtd_count};
		for (std::size_t idx = 0; idx < size_; ++idx, ++ridx)
		{
			Real r = unweight[ridx].Match(seq());
			lookup_[idx] = to_index[r];
		}
		std::cout << "Colored lookup ring." << std::endl;
	}

	Real Lookup(Index idx)
	{
		return to_real_[lookup_[idx]];
	}

	void Report()
	{
		std::unordered_map<Index, std::size_t> dist;
		for (std::size_t idx = 0; idx < size_; ++idx)
		{
			++dist[lookup_[idx]];
		}
		std::int64_t fair = size_ / dist.size();
		std::cout << "fair: " << fair << '\n';
		for (auto& [idx, count] : dist)
		{
			std::cout << to_real_[idx] << ' ' << count << " (" << std::fixed << std::setprecision(1) << std::abs(static_cast<double>(static_cast<std::int64_t>(count) - fair) * 100.0 / static_cast<double>(fair)) << "%)\n";
		}
	}
};

} // namespace balancer