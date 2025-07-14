#pragma once
#include <chash/service.hpp>

namespace chash
{

template<typename RealIter, typename IdIter, typename WeightIter>
std::optional<Service> Service::Make(RealId* buf,
                                     Index size,
                                     IdIter ids_begin,
                                     IdIter ids_end,
                                     RealIter reals_begin,
                                     WeightIter weights_begin)
{
	const auto need_size = LookupRequiredSize(std::distance(ids_begin, ids_end));
	if (need_size > size)
	{
		return std::nullopt;
	}
	std::optional<State> state = State::Make(
	        ids_begin, ids_end, reals_begin, weights_begin, SIDE_RINGS_COUNT, size);

	if (!state)
	{
		return std::nullopt;
	}

	Lookup lookup(buf, size);

	Service svc(std::move(state.value()), std::move(lookup));
	return svc;
}

Service::Service(State&& state, Lookup&& lookup) :
        state_{std::move(state)}, lookup_{std::move(lookup)} {}

template<typename IdIter, typename WeightIter>
void Service::Update(IdIter ids_begin, IdIter ids_end, WeightIter weights_begin)
{
	auto patch = state_.Update(ids_begin, ids_end, weights_begin);
	if (state_.Enabled())
	{
		lookup_.Update(patch);
	}
	else
	{
		lookup_.DisablingUpdate(patch.off);
	}
}

void Service::Adjust()
{
	state_.Adjust(lookup_.Stats(), [&](Index pos, RealId id) { lookup_.Enable(pos, id); }, [&](Index pos) { lookup_.Disable(pos); });
}

const RealId* Service::data() const
{
	return lookup_.data();
}

Index Service::size() const
{
	return lookup_.size();
}

} // namespace chash