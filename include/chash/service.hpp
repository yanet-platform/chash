#pragma once
#include <chash/config.hpp>
#include <chash/lookup.hpp>
#include <chash/state.hpp>

namespace chash
{

template<typename RealId = std::uint32_t>
class Service
{
	State<RealId> state_;
	Lookup<RealId> lookup_;
	Service(State<RealId>&& state, Lookup<RealId>&& lookup);

public:
	template<typename RealIter, typename IdIter, typename WeightIter>
	static std::optional<Service> Make(RealId* buf,
	                                   Index size,
	                                   IdIter ids_begin,
	                                   IdIter ids_end,
	                                   RealIter reals_begin,
	                                   WeightIter weights_begin);
	template<typename IdIter, typename WeightIter>
	void Update(IdIter ids_begin, IdIter ids_end, WeightIter weights_begin);
	void Adjust();
	const RealId* data() const;
	Index size() const;
};

} // namespace chash

#include <chash/service.ipp>