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
	std::set<IdHash> hids_;
	std::map<IdHash, Real> to_real_;
	Salt salt_;

	void PrintEntry(std::ostream& out, IdHash id) const
	{
		out << '{' << to_real_.at(id) << ", " << id << '}';
	}

public:
	Unweighted(const std::vector<Real>& reals, Salt salt) :
	        salt_{salt}
	{
		for (auto& real : reals)
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
	template<typename R>
	friend std::ostream& operator<<(std::ostream& o, const Unweighted<R>&);
};

template<typename Real>
std::ostream& operator<<(std::ostream& o, const Unweighted<Real>& ring)
{
	std::map<Real, IdHash> to_hid;
	for (auto& [h, r] : ring.to_real_)
	{
		to_hid[r] = h;
	}
	o << "Salt: " << ring.salt_;
	o << " Unweighted: {";
	auto hid = to_hid.begin();
	auto print = [](std::ostream& o, decltype(hid) & i) {
		o << '{' << i->first << ", " << i->second << '}';
	};
	print(o, hid);
	++hid;
	for (auto end = to_hid.end(); hid != end; ++hid)
	{
		o << ", ";
		print(o, hid);
	}

	o << '}';
	return o;
}

} // namespace balancer