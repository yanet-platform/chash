#pragma once

#include <map>
#include <random>
#include <set>
#include <vector>

#include "common/Crc32.h"

namespace balancer
{

using Index = std::size_t;
using Salt = std::uint32_t; // Salt is what differentiates one Unweighted from another
using IdHash = std::uint32_t; // IdHash = f(Real, Salt)

template<typename T>
IdHash Hash(const T& data, IdHash prev)
{
	return crc32_fast(static_cast<const void*>(&data), sizeof(data), prev);
}

template<typename Real>
class Unweighted
{
	std::set<IdHash> hids_;
	std::map<IdHash, Real> to_real_;

public:
	Unweighted(const std::vector<Real>& reals, Salt salt)
	{
		for (const auto& real : reals)
		{
			auto hid = Hash(real, salt);
			hids_.insert(hid);
			// Real comparing greater wins collision
			to_real_[hid] = std::max(to_real_[hid], real);
		}
	}
	Real Match(IdHash hash)
	{
		auto e = hids_.lower_bound(hash);
		return to_real_.at(*((e != hids_.end()) ? e : hids_.begin()));
	}
};

} // namespace balancer