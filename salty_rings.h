#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <random>
#include <unordered_map>
#include <vector>

#include "common/Crc32.h"

namespace balancer
{

using Weight = std::uint8_t;
static constexpr auto MAX_WEIGHT = 100;
static constexpr auto SEGMENTS_PER_WEIGHT_UNIT = 10;
static constexpr auto ROLLS_PER_REAL = MAX_WEIGHT * SEGMENTS_PER_WEIGHT_UNIT;

template<typename Index>
class RealSegments
{
	std::vector<Index> data_;
	std::size_t enabled_end_ = 0;

public:
	[[nodiscard]] Index Enable()
	{
		return data_[enabled_end_++];
	};
	[[nodiscard]] Index Disable()
	{
		return data_[--enabled_end_];
	};
	void Add(Index idx)
	{
		data_.push_back(idx);
		++enabled_end_;
	};
	std::size_t size() {return enabled_end_;}
};

template<typename Index>
class RandomElement
{
	std::mt19937 engine_;
	std::uniform_int_distribution<Index> uniform_;

public:
	RandomElement(Index end, std::size_t seed) :
	        engine_{seed},
	        uniform_{0, --end}
	{
	}
	Index Roll()
	{
		return uniform_(engine_);
	}
};

template<typename Real, typename Element = RandomElement<std::uint16_t>>
class Layout
{
	std::unordered_map<Real, Element> reals_;

public:
	template<typename Transform>
	Layout(const std::map<Real, Weight>& reals, Transform mutation)
	{
		for (auto& [real, weight] : reals)
		{
			reals_.emplace(std::piecewise_construct,
			               std::forward_as_tuple(real),
			               std::forward_as_tuple(reals.size(), mutation(real)));
		}
	}
	auto Roll(const Real& real)
	{
		return reals_.at(real).Roll();
	}
};

template<std::size_t Size, typename Real, typename Layout = Layout<Real>>
class NonConsistentRing
{
	using Index = decltype(Size);
	using RealId = std::uint16_t;
	Layout layout_;
	std::array<std::optional<std::size_t>, Size> heads_;
	std::array<std::size_t, Size> lookup_;
	std::vector<RealSegments<Index>> segments_;
	std::vector<Real> reals_;

public:
	NonConsistentRing(const std::map<Real, Weight>& reals) :
	        layout_(reals, [](const Real& real) {
				return crc32_fast(static_cast<const void*>(&real), sizeof(real), 0);
	        })
	{
		for (auto& real : reals)
		{
			reals_.push_back(real.first);
		}
		PickHeads(reals);
		FillTails();
		ApplyWeights(reals);
	}

	Real Lookup(Index idx)
	{
		return reals_[lookup_[idx]];
	}

	std::size_t PickHeads(const std::map<Real, Weight>& reals)
	{
		std::size_t holes{};
		for (std::size_t r = 0; r < ROLLS_PER_REAL; ++r)
		{
			for (std::size_t id = 0; id < reals.size(); ++id)
			{
				const auto& roll = layout_.Roll(*(reals_.begin() + id));
				if (!heads_[roll])
				{
					heads_[roll] = id;
					segments_.at(id).Add(roll);
				}
				else
				{
					++holes;
				}
			}
		}
		return holes;
	}

	void FillTails()
	{
		auto head = std::find_if(heads_.begin(),
		                         heads_.end(),
		                         [](const auto& e) {
			                         return bool(e);
		                         });
		Index pos = std::distance(heads_.begin(), head);
		RealId color;
		for (std::size_t steps = 0; steps < Size; ++steps, Inc(pos))
		{
			if (auto head = heads_[pos])
			{
				color = head.value();
			}

			lookup_[pos] = color;
		}
	}

	void ApplyWeights(const std::map<Real, Weight>& reals)
	{
		for (const auto& [real, weight] : reals)
		{
			auto& segments = segments_.at(real);
			const std::size_t target = weight * SEGMENTS_PER_WEIGHT_UNIT;
			while (segments.size() > target)
			{
				DisableSegment(segments.Disable());
			}
		}
	}

	/* @brief segment starting at head and following segments recolored with
	 * this one. (Leaving recolored segments would prevent us turning the real
	 * off completely)
	 */
	void DisableSegment(Index head)
	{
		auto id = lookup_[head];
		auto shadow = PickShadowTint(head);
		head = SetSegment(shadow, head);
		OvercastShadow(shadow, id, head);
	}

	Index& Inc(Index& pos) const
	{
		++pos;
		if (pos >= Size)
		{
			pos = 0;
		}
		return pos;
	}

	Index& Dec(Index& pos) const
	{
		if (!pos)
		{
			pos = Size - 1;
		}
		else
		{
			--pos;
		}
		return pos;
	}

	Index Next(Index pos) const
	{
		return Inc(pos);
	}

	RealId PickShadowTint(Index pos) const
	{
		auto color = lookup_[pos];
		for (std::size_t steps = 0; steps < Size; ++steps, Dec(pos))
		{
			if (auto tint = lookup_[pos]; tint != color)
			{
				return tint;
			}
		}
		abort();
	}

	Index SetSegment(RealId id, Index head)
	{
		lookup_[head] = id;
		auto pos = Next(head);
		for (; !heads_[pos]; Inc(pos))
		{
			lookup_[pos] = id;
		}
		return pos;
	}

	void OvercastShadow(RealId darker, RealId shadow, Index start)
	{
		while ((lookup_[start] == shadow) && (heads_[start].value() != shadow))
		{
			start = SetSegment(darker, start);
		}
	}
};

} // namespace