#pragma once

#include <map>
#include <random>
#include <set>
#include <vector>

#include "../common/Crc32.h"

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

template<>
IdHash Hash(const std::string& data, IdHash prev)
{
	return crc32_fast(static_cast<const void*>(data.c_str()), data.size() * sizeof(std::string::value_type), prev);
}

template<typename Real>
class Unweighted
{
	std::map<IdHash, Real> to_real_;
	Salt salt_;

public:
	Unweighted(const std::vector<Real>& reals, Salt salt) :
	        salt_{salt}
	{
		for (auto& real : reals)
		{
			auto hid = Hash(real, salt);
			// Real comparing greater wins collision
			if (to_real_.find(hid) != to_real_.end())
			{
				to_real_[hid] = std::max(to_real_[hid], real);
			}
			else
			{
				to_real_[hid] = real;
			}
		}
	}

	Real Match(IdHash hash)
	{
		auto e = to_real_.lower_bound(hash);
		return (e != to_real_.end()) ? e->second : to_real_.begin()->second;
	}
};

} // namespace balancer