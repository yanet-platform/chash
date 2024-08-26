#include <array>
#include <bitset>
#include <chrono>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <random>
#include <set>
#include <thread>
#include <vector>

#include "common/static_vector.h"

namespace balancer
{

using ServiceId = std::uint16_t;
using RealId = std::uint16_t;
using LayoutHash = std::uint16_t;
using QueryHash = std::uint32_t;
using Weight = std::uint16_t;
using NormalizedWeight = std::double_t;

class RealSegments
{
	std::vector<LayoutHash> data_;
	LayoutHash enabled_end_ = 0;

public:
	[[nodiscard]] LayoutHash Enable()
	{
		return data_[enabled_end_++];
	};
	[[nodiscard]] LayoutHash Disable()
	{
		return data_[--enabled_end_];
	};
	void Add(LayoutHash idx)
	{
		data_.push_back(idx);
		++enabled_end_;
	};
};

struct Real
{
	Weight weight = {};
	RealSegments segments;
};

const constexpr auto REALS_MAX = 1024;

using RealsConfig = std::map<RealId, Weight>;

class Service
{
	static constexpr auto REALS_MAX = 1024;
	static constexpr auto SERVICE_SIZE = std::numeric_limits<LayoutHash>::max();
	static constexpr auto SEGMENT_WIDTH = 8;
	static constexpr auto ROLLS_PER_SEGMENT = SEGMENT_WIDTH / 2;
	static constexpr auto SEED = 42;
	static constexpr LayoutHash STARTING_ROLL = 1;
	bool running_ = false;
	Weight weight_norm_ = {};
	std::array<std::optional<RealId>, SERVICE_SIZE> heads_;
	std::array<RealId, SERVICE_SIZE> reference_ = {};
	std::array<RealId, SERVICE_SIZE> lookup_;

	std::array<RealSegments, REALS_MAX> segs_;

	std::map<RealId, std::size_t> segment_chains_mapping_;
	std::set<std::size_t> unused_segment_chains_idxs_;

	std::map<RealId, RealSegments> segments_;
	std::map<RealId, Weight> config_;

	void OvercastShadow(RealId darker, RealId shadow, LayoutHash start)
	{
		while ((lookup_[start] == shadow) && (heads_[start] != shadow))
		{
			start = SetSegment(darker, start);
		}
	}

	LayoutHash SetSegment(RealId id, LayoutHash head)
	{
		lookup_[head++] = id;
		for (int i = 1; i < SEGMENT_WIDTH; ++i, ++head)
		{
			if (head >= SERVICE_SIZE)
			{
				head = 0;
			}

			if (heads_[head])
			{
				break;
			}

			lookup_[head] = id;
		}
		return head;
	}

	void EnableSegment(LayoutHash head)
	{
		auto id = heads_[head].value();
		auto shadow = lookup_[head];
		head = SetSegment(id, head);
		OvercastShadow(id, shadow, head);
	}

	RealId PickShadowTint(LayoutHash for_position)
	{
		auto color = lookup_[for_position];
		for (LayoutHash steps = 0; steps < SERVICE_SIZE; ++steps)
		{
			if (!for_position)
			{
				for_position = SERVICE_SIZE;
			}

			if (auto tint = lookup_[--for_position]; tint != color)
			{
				return tint;
			}
		}
		abort();
	}

	/* @brief segment starting at head and following segments recolored with
	 * this one. (Leaving recolored segments would prevent us turning the real
	 * off completely)
	 */
	void DisableSegment(LayoutHash head)
	{
		auto id = lookup_[head];
		auto shadow = PickShadowTint(head);
		head = SetSegment(shadow, head);
		OvercastShadow(shadow, id, head);
	}

	void FillTails()
	{
		auto head = std::find_if(heads_.begin(),
		                         heads_.end(),
		                         [](const auto& e) {
			                         return bool(e);
		                         });
		auto pos = std::distance(heads_.begin(), head);
		RealId color;
		for (std::size_t steps = 0; steps < SERVICE_SIZE; ++steps, ++pos)
		{
			if (pos >= SERVICE_SIZE)
			{
				pos = 0;
			}

			if (auto head = heads_[pos])
			{
				color = head.value();
			}

			reference_[pos] = color;
		}
	}

	std::size_t PickHeads(const RealsConfig& reals)
	{
		std::size_t holes{};
		auto segments_per_real = SERVICE_SIZE / reals.size();
		std::cout << "segments_per_real: " << segments_per_real << std::endl;
		std::mt19937 engine(SEED);
		std::uniform_int_distribution<LayoutHash> uniform(0, std::numeric_limits<LayoutHash>::max() - 1);
		LayoutHash roll;

		for (std::size_t i = 0; i < segments_per_real; ++i)
		{
			for (auto [id, weight] : reals)
			{
				(void)weight;
				roll = uniform(engine);
				if (!heads_[roll])
				{
					heads_[roll] = id;
					segments_[id].Add(roll);
				}
				else
				{
					++holes;
				}
			}
		}
		return holes;
	}

	Weight Norm(const std::map<RealId, Weight>& reals)
	{
		return std::max_element(reals.begin(),
		                        reals.end(),
		                        [](const auto& a,
		                           const auto& b) {
			                        return a.second < b.second;
		                        })
		        ->second;
	}

	void Renormalize(Weight norm)
	{
	}

	void SetWeight(RealId id, Weight weight)
	{
	}

public:
	void UpdateWeights(std::map<RealId, Weight>&& changes)
	{
	}

	Service(RealsConfig reals) :
	        weight_norm_{Norm(reals)},
	        heads_{std::nullopt}
	{
		std::size_t holes = PickHeads(reals);
		FillTails();
		std::cout << "Layout done, " << holes << " holes." << std::endl;
		std::copy(reference_.begin(), reference_.end(), lookup_.begin());
		UpdateWeights(reals);
	}

	RealId Lookup(LayoutHash hash) const
	{
		return lookup_[hash];
	}

	operator bool() const { return running_; }

	std::map<RealId, Weight> Reference()
	{
		std::map<RealId, Weight> m;
		std::for_each(std::begin(reference_),
		              std::end(reference_),
		              [&m](const RealId& id) {
			              m[id]++;
		              });
		return m;
	}

	std::map<RealId, Weight> Layout()
	{
		std::map<RealId, Weight> m;
		std::for_each(std::begin(lookup_),
		              std::end(lookup_),
		              [&m](const RealId& id) {
			              m[id]++;
		              });
		return m;
	}
};

} // namespace balancer