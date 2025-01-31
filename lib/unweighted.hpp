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
	std::map<IdHash, RealId> to_id_;
	std::map<RealId, IdHash> to_hash_;

public:
	Unweighted() = default;

	template<typename Real>
	Unweighted(const std::unordered_map<RealId, Real>& reals, Salt salt)
	{
		for (auto& [id, real] : reals)
		{
			auto hid = CalcHash(real, salt);
			// Real comparing greater wins collision
			if (auto it = to_id_.find(hid); it != to_id_.end())
			{
				auto& real_old = reals.at(to_id_.at(hid));
				if (real_old < real)
				{
					to_id_[hid] = id;
				}
			}
			else
			{
				to_id_[hid] = id;
			}
		}

		for (auto [hash, id] : to_id_)
		{
			to_hash_.emplace(id, hash);
		}
	}

	RealId Match(IdHash hash)
	{
		auto e = to_id_.lower_bound(hash);
		return (e != to_id_.end()) ? e->second : to_id_.begin()->second;
	}

#if UNUSED
	std::optional<RealId> Substitute(RealId id)
	{
		auto hit = to_hash_.find(id);
		if (hit == to_hash_.end() || (to_id_.size() == 1))
		{
			return std::nullopt;
		}
		auto it = to_id_.find(hit->second);
		if (it == to_id_.begin())
		{
			it = to_id_.end();
		}
		return std::prev(it)->second;
	}
#endif

	bool contains(RealId id) const
	{
		return std::any_of(
		        to_id_.begin(),
		        to_id_.end(),
		        [&](const auto& it) {
			        return it.second == id;
		        });
	}

	void erase(RealId id)
	{
		IdHash hid = to_hash_.at(id);
		to_hash_.erase(id);
		to_id_.erase(hid);
	}

	std::size_t size() const
	{
		return to_id_.size();
	}

	bool empty() const
	{
		return size() == 0;
	}
};

} // namespace chash