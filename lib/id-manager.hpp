#pragma once
#include <set>
#include <unordered_map>

#include "common.hpp"

namespace chash
{

template<typename Real>
class IdManager
{
	std::unordered_map<RealId, Real> to_real_;
	std::unordered_map<Real, RealId> to_id_;
	RealId min_unassigned_ = 0;
	std::set<RealId> freed_;

public:
	RealId Assign(const Real& real)
	{
		if (auto it = to_id_.find(real); it != to_id_.end())
		{
			return it->second;
		}

		RealId id;
		if (!freed_.empty())
		{
			id = *freed_.begin();
			freed_.erase(freed_.begin());
		}
		else
		{
			id = min_unassigned_;
			++min_unassigned_;
		}

		return id;
	}

	void Release(const Real& real)
	{
		if (auto it = to_id_.find(real); it != to_id_.end())
		{
			freed_.insert(it->second);
			to_real_.erase(it->second);
			to_id_.erase(real);
		}
	}

	RealId GetId(const Real& real)
	{
		return to_id_.at(real);
	}

	const Real& GetReal(RealId id)
	{
		return to_real_.at(id);
	}

	std::size_t size()
	{
		return to_id_.size();
	}
};

} // namespace chash