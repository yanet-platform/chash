#pragma once
#include <iomanip>
#include <iostream>

#include <chash.hpp>

#include "printers.hpp"

template<typename Real>
class ChattyChash : public chash::Chash<Real>
{
	using Base = chash::Chash<Real>;
	using RealId = typename chash::RealId;
	using Key = typename Base::Key;
	using Index = typename chash::Index;
	using Base::enabled_;
	using Base::info_;
	using Base::lookup_;
	using Base::lookup_mask;
	using Base::lookup_size;
	using Base::slices_per_weight_unit_;
	using Base::to_real_;

public:
	ChattyChash(const std::map<Real, chash::RealConfig>& reals,
	            std::size_t uwtd_count,
	            std::uint8_t lookup_bits,
				std::pmr::memory_resource* mem) :
	        Base(reals, uwtd_count, lookup_bits, mem)
	{}
	void Report()
	{
		std::unordered_map<RealId, Index> dist;
		for (auto id : Base::lookup_)
		{
			++dist[id];
		}
		Index norm = std::numeric_limits<Index>::min();
		for (auto& [_, count] : dist)
		{
			norm = std::max(norm, count);
		}

		std::cout << "Max: " << norm << "\n";
		std::cout << "Fair: " << Base::Fair() << '\n';
		std::cout << "Id         Alloc    Active  Enabled   \%Active  \%Enabled Current Target\n";
		for (auto& [id, info] : Base::info_)
		{
			std::cout << std::setw(10) << std::left << to_real_.at(id) << " "
			          << std::setw(8) << info_.at(id).heads.size()
			          << std::setw(7) << std::right << dist[id]
			          << std::setw(9) << std::right << Parenthesize(info.enabled)
			          << std::setw(10) << std::right << std::fixed << std::setprecision(1) << static_cast<double>(dist[id]) * 100.0 / info.heads.size() << ' '
			          << std::setw(9) << std::right << Parenthesize(std::fixed, std::setprecision(1), static_cast<double>(info.enabled) * 100.0 / info.heads.size()) << " "
			          << std::setw(10) << std::right << static_cast<double>(dist[id]) * 100 / norm;
			std::cout << std::setw(9) << std::right << static_cast<int>(info_.at(id).desired / slices_per_weight_unit_)
			          << "\n";
		}
	}

	void ReportFrag()
	{
		using SegLength = Index;
		std::vector<std::map<SegLength, std::size_t>> r(info_.size());
		std::size_t cell{};
		while (!enabled_[cell] && cell < lookup_.size())
		{
			++cell;
		}
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
			std::cout << std::setw(10) << std::left << to_real_.at(i);
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
		          << "Lookup mask:" << std::hex << lookup_mask << std::dec << '\n'
		          << "Segments at 100 weight: " << slices_per_weight_unit_ * 100 << '\n';
	}
};