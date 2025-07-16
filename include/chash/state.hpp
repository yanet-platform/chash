#pragma once
#include <unordered_map>

#include <chash/config.hpp>
#include <chash/patch.hpp>
#include <chash/real.hpp>

namespace chash
{

#if INDEPENDENCE
class SegmentIterator
{
	std::unordered_map<RealId, RealInfo>::const_iterator rit_;
	std::vector<Index>::const_iterator sit_;

public:
	operator++
};
#endif

template<typename RealId>
class State
{
	std::unordered_map<RealId, Real<RealId>> reals_;
	const Index lookup_size_;
	const Index segments_per_weight_ = DEFAULT_SEGMENTS_PER_WEIGHT;
	Index reals_active_ = 0;
	Index total_weight_ = 0;

	void Rebalance(Index target);
	State(Index lookup_size) : lookup_size_(lookup_size) {}

public:
	template<typename RealIter, typename IdIter, typename WeightIter>
	static std::optional<State> Make(
	        IdIter ids_begin,
	        IdIter ids_end,
	        RealIter reals_begin,
	        WeightIter weights_begin,
	        Index side_rings_count,
	        // Index segments_per_weight,
	        Index lookup_size);

	template<typename IdIter, typename WeightIter>
	Patch<RealId> Update(IdIter ids_begin, IdIter ids_end, WeightIter weights_begin);
	template <typename enable_t, typename disable_t>
	void Adjust(const std::unordered_map<RealId, Index>& stats, enable_t&& enable, disable_t&& disable);
#if INDEPENDENCE
	SegmentIterator cbegin();
	SegmentIterator cend();
#endif
	bool Enabled() const;
	bool Disabled() const;
};

} // namespace chash

#include <chash/state.ipp>