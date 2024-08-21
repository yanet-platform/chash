#include <array>
#include <bitset>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>

#include "Crc32.h"

namespace balancer
{

using ServiceId = std::uint16_t;
using RealId = std::uint16_t;
using LayoutHash = std::uint16_t;
using QueryHash = std::uint32_t;
using Weight = std::uint16_t;

struct Real
{
	RealId id;
	Weight weight = {};
	LayoutHash current = {};
	bool enabled = false;
	LayoutHash NextHash()
	{
		current = crc32_4bytes(&id, sizeof(LayoutHash), current);
		return current;
	}
	bool operator<(const Real& other)
	{
		return id < other.id;
	}
};

class Service
{
	static constexpr auto SERVICE_SIZE = std::numeric_limits<LayoutHash>::max();
	static constexpr auto ROLL_SEGMENT_WIDTH = 10;
	bool running_ = false;
	std::bitset<SERVICE_SIZE> is_head_ = {false};
	std::array<RealId, SERVICE_SIZE> segments_;
	std::map<RealId, Real> info_;

	LayoutHash ChoosePosition(RealId id)
	{
		return info_.at(id).NextHash();
	}

	/* Scan for index starting from position, wrapping at SERVICE_SIZE.
	 * Steps are limited to SERVICE_SIZE in case no index sutisfying
	 * condition are present.
	 */
	template<typename T, typename F>
	std::optional<T> PickFittingPosition(T position, F condition)
	{
		std::size_t steps{};
		while (!condition(position))
		{
			if (++position == SERVICE_SIZE)
			{
				position = 0;
			}
			if (++steps == SERVICE_SIZE)
			{
				return std::nullopt;
			}
		}
		return position;
	}

public:
	std::optional<RealId> GetReal(LayoutHash hash)
	{
		auto pos = PickFittingPosition(hash % SERVICE_SIZE,
		                               [this](auto idx) {
			                               return !info_[segments_[idx]].enabled;
		                               });
		if (!pos)
		{
			return std::nullopt;
		}
		return segments_[*pos];
	}

	operator bool() { return running_; }

	void AddSegments(RealId id, std::size_t count)
	{
		for (std::size_t i = 0; i < count; ++i)
		{
			const auto head = ChoosePosition(id);
			is_head_[head] = true;
			segments_[head] = id;
			for (auto tail = head + 1; tail < ROLL_SEGMENT_WIDTH && !is_head_[tail]; ++tail)
			{
				segments_[tail] = id;
			}
		}
	}

	void SetWeight(RealId id, Weight weight)
	{
		
	}

	void Disable(RealId id)
	{
		if (info_.contains(id))
		{
			info_[id].enabled = false;
		}
	}

	void Enable(RealId id)
	{
		if (info_.contains(id))
		{
			info_[id].enabled = true;
		}
	}
};

#if 0
template<std::size_t service_size_max,
         std::size_t service_count_max,
         std::size_t reals_size_max,
         typename ConsistentHasher>
class Balancer
{
	using Service = balancer::Service<service_size_max>;
	ConsistentHasher hasher_;
	std::array<RealInfo, reals_size_max> real_;
	std::array<Service, service_count_max> service_;
	LayoutHash LayNext(RealId id)
	{
		real_[id].current = hasher_(real_[id].current);
		return real_[id].current;
	}

public:
	Service& GetService(ServiceId id) { return service_[id]; }
	RealId GetReal(ServiceId id, LayoutHash hash)
	{
		return GetService(id).GetReal(hash);
	}
};
#endif

} // namespace balancer