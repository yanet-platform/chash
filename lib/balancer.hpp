#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <thread>
#include <unordered_map>
#include <vector>

#include <chash.hpp>

namespace chash
{

struct Logger
{
	template<typename... Args>
	void Error(Args... args)
	{
		(std::cerr << "Error: " << ... << args) << "\n";
	}

	template<typename... Args>
	void Debug(Args... args)
	{
		(std::cerr << "Debug: " << ... << args) << "\n";
	}
};

class Service : Logger
{
	using Index = std::uint32_t;
	using RealId = std::uint32_t;
	WeightUpdater state_;
	std::vector<bool> enabled_;
	bool disabled_ = true;
	std::vector<RealId> lookup_;

	Service(WeightUpdater&& st) :
	        state_{std::move(st)},
	        enabled_(state_.LookupSize() + 1, false),
	        lookup_(state_.LookupSize(), 0)
	{
		enabled_.back() = true; // Marker for updater to signal end of ring
	}

public:
	template<typename IdIter, typename RealIter, typename WeightIter>
	static std::optional<Service> MakeService(IdIter ids_begin,
	                                          IdIter ids_end,
	                                          RealIter reals_begin,
	                                          WeightIter weights_begin)
	{
		auto oupdater = MakeWeightUpdater(
		        ids_begin,
		        ids_end,
		        reals_begin,
		        weights_begin,
		        200,
		        16);
		if (!oupdater)
		{
			return std::nullopt;
		}
		Service s(std::move(oupdater.value()));
		s.InitLookup();
		// s.AdjustState();
		return s;
	}

	void InitLookup()
	{
		state_.InitLookup(lookup_.begin(), enabled_.begin());
	}

	void __attribute__((noinline)) UpdateLookup(const Patch& patch)
	{
		const auto& [on, off, offstart] = patch;
		// no enabled head to split at means service is disabled
		if (state_.Disabled())
		{
			for (auto pos : off)
			{
				enabled_[pos] = false;
			}
			if (lookup_.front() != state_.Invalid())
			{
				std::fill(lookup_.begin(), lookup_.end(), state_.Invalid());
				state_.track.clear();
				state_.track[state_.Invalid()] = lookup_.size();
			}
			return;
		}

		for (const auto& [pos, id] : on)
		{
			enabled_[pos] = true;
			lookup_[pos] = id;
		}

		for (const auto& [pos, id] : on)
		{
			const RealId old = lookup_[pos];
			Index i = pos + 1;
			for (; !enabled_[i]; ++i)
			{
				lookup_[i] = id;
			}
			const Index l = i - pos;
			state_.track[old] -= l;
			state_.track[id] += l;
		}

		// Ring seam
		if (!enabled_.front() && lookup_.front() != lookup_.back())
		{
			const RealId old = lookup_.front();
			const RealId tint = lookup_.back();
			Index i = 0;
			for (; !enabled_[i]; ++i)
			{
				lookup_[i] = tint;
			}
			state_.track[tint] += i;
			state_.track[old] -= i;
		}

		if (off.empty())
		{
			return;
		}

		auto off_split = std::lower_bound(off.begin(), off.end(), offstart.value());

		auto turn_off = [&](const RealId& h) {
			const RealId tint = lookup_[PrevRingPosition(lookup_.size(), h)];
			const RealId old = lookup_[h];
			enabled_[h] = false;
			Index i = h;
			for (; !enabled_[i]; ++i)
			{
				lookup_[i] = tint;
			}
			const Index l = i - h;
			state_.track[tint] += l;
			state_.track[old] -= l;

		};

		for (auto hit = off_split; hit != off.cend(); ++hit)
		{
			turn_off(*hit);
		}

		// Ring seam
		if (!enabled_.front() && lookup_.front() != lookup_.back())
		{
			const RealId old = lookup_.front();
			Index i = 0;
			for (; !enabled_[i]; ++i)
			{
				lookup_[i] = lookup_.back();
			}
			state_.track[lookup_.back()] += i;
			state_.track[old] -= i;
		}

		for (auto hit = off.cbegin(); hit != off_split; ++hit)
		{
			turn_off(*hit);
		}

		disabled_ = false;
	}

	void AdjustState()
	{
#if ADJUST
		Patch patch = state_.Adjust();
		UpdateLookup(patch);
#endif
	}

	template<typename IdIter, typename WeightIter>
	Patch UpdateState(IdIter ids_begin, IdIter ids_end, WeightIter weights_begin)
	{
		return state_.Update(ids_begin, ids_end, weights_begin);
	}
	auto Lookup()
	{
		return std::pair{lookup_.data(), lookup_.size()};
	}
	std::vector<RealId>&& MoveLookup()
	{
		return std::move(lookup_);
	}
};

class Balancer : Logger
{
	using ServiceId = std::uint32_t;
	using RealId = std::uint32_t;
	using PatchBundle = std::unordered_map<ServiceId, Patch>;
	std::unordered_map<ServiceId, Service> services_;
	std::atomic<bool> need_updater_;
	PatchBundle patches_;
	std::vector<std::vector<RealId>> stale_lookups_;

public:
	template<typename IdIter, typename RealIter, typename WeightIter>
	bool AddService(ServiceId id,
	                IdIter ids_begin,
	                IdIter ids_end,
	                RealIter reals_begin,
	                WeightIter weights_begin)
	{
		if (services_.find(id) != services_.end())
		{
			Error("Service already exists");
			return false;
		}
		auto oservice = Service::MakeService(
		        ids_begin,
		        ids_end,
		        reals_begin,
		        weights_begin);
		if (!oservice)
		{
			Error("Failed to add service ", id, " to balancer");
			return false;
		}
		services_.emplace(id, std::move(oservice.value()));
		return true;
	}

	bool Contains(ServiceId id)
	{
		return (services_.find(id) != services_.end());
	}

	void ClearServices()
	{
		services_.clear();
		stale_lookups_.reserve(stale_lookups_.size() + services_.size());
		for (auto& svc : services_)
		{
			stale_lookups_.emplace_back(svc.second.MoveLookup());
		}
		patches_.clear();
	}

	void ClearStale()
	{
		stale_lookups_.clear();
	}

	std::size_t size() const
	{
		return services_.size();
	}

	bool empty()
	{
		return size() == 0;
	}

	auto Lookup(ServiceId id)
	{
		return services_.at(id).Lookup();
	}

	template<typename IdIter, typename WeightIter>
	void UpdateWeights(ServiceId id, IdIter ids_begin, IdIter ids_end, WeightIter weights_begin)
	{
		if (auto it = services_.find(id); it != services_.end())
		{
			auto& [sid, service] = *it;

			patches_[sid] = service.UpdateState(ids_begin, ids_end, weights_begin);
		}
		else
		{
			Error("Service ", id, " not found");
		}
	}
	void UpdateLookups()
	{
		for (auto& [sid, patch] : patches_)
		{
			if (services_.find(sid) == services_.end())
			{
				Error("Patch for nonexistent service ", sid);
			}
			services_.at(sid).UpdateLookup(patch);
		}
	}
};

} // namespace chash