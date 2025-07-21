#pragma once
#include <chash/real.hpp>

namespace chash
{

template<typename RealId>
class SegmentIterator
{
	typename std::unordered_map<RealId, Real<RealId>>::const_iterator rit_;
	std::vector<Index>::const_iterator sit_;

	const typename std::unordered_map<RealId, Real<RealId>>::const_iterator rit_end_;

public:
	SegmentIterator(typename std::unordered_map<RealId, Real<RealId>>::const_iterator b,
	                typename std::unordered_map<RealId, Real<RealId>>::const_iterator e) :
	        rit_{b},
	        rit_end_{e}
	{
		while (rit_ != rit_end_ && rit_->second.Disabled())
		{
			++rit_;
		}

		if (rit_ != rit_end_)
		{
			sit_ = rit_->second.cbegin();
		}
	}

	SegmentIterator& operator++()
	{
		if (rit_ == rit_end_)
		{
			return *this;
		}
		++sit_;
		if (sit_ == rit_->second.cend())
		{
			++rit_;
			while (rit_ != rit_end_ && rit_->second.Disabled())
			{
				++rit_;
			}

			if (rit_ != rit_end_)
			{
				sit_ = rit_->second.cbegin();
			}
		}
		return *this;
	}

	SegmentIterator operator++(int)
	{
		auto tmp = *this;
		++(*this);
		return tmp;
	}

	std::pair<Index, RealId> operator*() const
	{
		return {*sit_, rit_->first};
	}

	bool operator==(const SegmentIterator& other) const
	{
		return ((rit_ == rit_end_) && (other.rit_ == other.rit_end_)) ||
		       ((rit_ == other.rit_) && (sit_ == other.sit_));
	}

	bool operator!=(const SegmentIterator& other) const
	{
		return !(*this == other);
	}

	Index pos() const
	{
		return *sit_;
	}

	RealId id() const
	{
		return rit_->first;
	}
};

} // namespace chash