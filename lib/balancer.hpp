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
	        enabled_(state_.LookupSize(), false),
	        lookup_(state_.LookupSize(), 0)
	{
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
		//s.AdjustState();
		return s;
	}

	void InitLookup()
	{
		state_.InitLookup(lookup_.begin(), enabled_.begin());
	}

	void __attribute__ ((noinline)) UpdateLookup(const Patch& patch)
	{
		const auto& [ops, ostart] = patch;
		// no enabled head to split at means service is disabled
		if (!ostart)
		{
			for (auto& [pos, op] : ops)
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

		if (ops.size() == 0)
		{
			return;
		}
		Index start = ostart.value();

		auto ops_split = ops.lower_bound(start);

		auto up = [&](Index current, const PatchOperation<DefaultConfig>& op, Index next) {
			RealId tint;
			if (op.on)
			{
				tint = op.id;
			}
			else
			{
				tint = lookup_[PrevRingPosition(lookup_.size(), current)];
			}

			enabled_[current] = op.on;
			//const RealId old = std::exchange(lookup_[current], tint);
			//state_.track[old] -= 1;
			//state_.track[tint] += 1;

			Index pos = NextRingPosition(lookup_.size(), current);
			for (;
			     !enabled_[pos] && pos != next;
			     pos = NextRingPosition(lookup_.size(), pos))
			{
				//state_.track[lookup_[pos]] -= 1;
				lookup_[pos] = tint;
				//state_.track[tint] += 1;
			}

			// const auto cnt = (next > current) ? next - current : lookup_.size() - next + current;
			// state_.track[old] -= cnt;
			// state_.track[tint] += cnt;
		};

		for (auto op = ops_split; op != ops.cend(); ++op)
		{
			auto next_op = std::next(op);
			if (next_op == ops.cend())
			{
				next_op = ops.begin();
			}
			up(op->first, op->second, next_op->first);
		}

		for (auto op = ops.cbegin(); op != ops_split; ++op)
		{
			auto next_op = std::next(op);
			if (next_op == ops.cend())
			{
				next_op = ops.begin();
			}
			up(op->first, op->second, next_op->first);
		}

		// std::stringstream ss;
		// for (auto [id, cnt] : state_.track)
		// {
		// 	ss << id << ": " << cnt << "\n";
		// }

		// Error(ss.str());

		disabled_ = false;
	}

	void AdjustState()
	{
		Patch patch = state_.Adjust();
		UpdateLookup(patch);
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