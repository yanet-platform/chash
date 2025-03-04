#pragma once

#include <algorithm>
#include <map>
#include <optional>
#include <random>
#include <vector>

#include "hash.hpp"

namespace chash
{

template<typename RealId>
class Unweighted
{
	std::vector<RealId> lookup_;

	template<typename Real>
	static std::map<IdHash, RealId> Temporary(
	        const Real* reals,
	        const RealId* ids,
	        std::size_t cnt,
	        Salt salt,
			std::size_t size)
	{
		std::map<IdHash, RealId> to_id;
		std::map<IdHash, std::size_t> seen;
		for (std::size_t i = 0; i < cnt; ++i)
		{
			const Real& real = reals[i];
			const RealId& id = ids[i];
			auto hid = CalcHash(real, salt) % size;
			// Real comparing greater wins collision
			if (auto it = to_id.find(hid); it != to_id.end())
			{
				auto& real_old = reals[seen.at(hid)];
				if (real_old < real)
				{
					to_id[hid] = id;
					seen[hid] = i;
				}
			}
			else
			{
				to_id[hid] = id;
				seen[hid] = i;
			}
		}
		return to_id;
	}

	std::unordered_set<RealId> InitLookup(const std::map<IdHash, RealId>& guide, std::size_t size)
	{
		RealId tint = std::prev(guide.end())->second;
		std::unordered_set<RealId> result;
		result.insert(tint);
		auto next = guide.begin();
		for (std::size_t i = 0; i < size; ++i)
		{
			if ((next != guide.end()) && (next->first == i))
			{
				tint = next->second;
				result.insert(tint);
				++next;
			}
			lookup_.push_back(tint);
		}
		return result;
	}

	Unweighted(std::size_t size)
	{
		lookup_.reserve(size);
	}

public:
	template<typename Real>
	static std::pair<Unweighted, std::unordered_set<RealId>> Make(
	        const Real* reals,
	        const RealId* ids,
	        std::size_t cnt,
	        Salt salt,
	        std::size_t size)
	{
		std::map<IdHash, RealId> mapping = Temporary(reals, ids, cnt, salt, size);
		Unweighted ring(size);

		return std::pair<Unweighted, std::unordered_set<RealId>>{ ring, ring.InitLookup(mapping, size) };
	}

	RealId Match(IdHash hash)
	{
		return lookup_[hash % lookup_.size()];
	}

};

} // namespace chash