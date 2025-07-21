#pragma once
#include <chash/service.hpp>

namespace chash
{

template<typename RealId>
template<typename RealIter, typename IdIter, typename WeightIter>
std::optional<Service<RealId>> Service<RealId>::Make(RealId* buf,
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
	std::optional<State<RealId>> state = State<RealId>::Make(
	        ids_begin, ids_end, reals_begin, weights_begin, SIDE_RINGS_COUNT, size);

	if (!state)
	{
		return std::nullopt;
	}

	Lookup lookup(buf, size);
	lookup.Init(state->cbegin(), state->cend());

	Service svc(std::move(state.value()), std::move(lookup));
	svc.Adjust();
	return svc;
}

template<typename RealId>
Service<RealId>::Service(State<RealId>&& state, Lookup<RealId>&& lookup) :
        state_{std::move(state)}, lookup_{std::move(lookup)} {}

template<typename RealId>
template<typename IdIter, typename WeightIter>
void Service<RealId>::Update(IdIter ids_begin, IdIter ids_end, WeightIter weights_begin)
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

template<typename RealId>
void Service<RealId>::Adjust()
{
	state_.Adjust(lookup_.Stats(), [&](Index pos, RealId id) { lookup_.Enable(pos, id); }, [&](Index pos) { lookup_.Disable(pos); });
}

template<typename RealId>
const RealId* Service<RealId>::data() const
{
	return lookup_.data();
}

template<typename RealId>
Index Service<RealId>::size() const
{
	return lookup_.size();
}

} // namespace chash