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
	std::function<void()> on_ready_;
	std::function<void()> on_not_ready_;

	Service(WeightUpdater&& st) :
	        state_{std::move(st)},
	        enabled_(state_.LookupSize(), false),
	        lookup_(state_.LookupSize(), 0)
	{
	}

public:
	template<typename IdIter, typename RealIter, typename WeightIter>
	static std::optional<Service> MakeService(IdIter ids_begin, IdIter ids_end, RealIter reals_begin, WeightIter weights_begin)
	{
		auto oupdater = MakeWeightUpdater(
		        ids_begin,
		        ids_end,
		        reals_begin,
		        weights_begin,
		        1000,
		        40);
		if (!oupdater)
		{
			return std::nullopt;
		}
		Service s(std::move(oupdater.value()));
		s.InitLookup();
		return s;
	}

	void Disable()
	{
		disabled_ = true;
		if (on_not_ready_)
		{
			on_not_ready_();
		}
	}

	void InitLookup()
	{
		state_.InitLookup(lookup_.begin(), enabled_.begin());
		disabled_ = lookup_.front() == state_.Invalid();
		if (disabled_)
		{
			if (on_not_ready_)
			{
				on_not_ready_();
			}
		}
		else
		{
			if (on_ready_)
			{
				on_ready_();
			}
		}
	}

	void UpdateLookup(const Patch& patch)
	{
		const auto& [ops, ostart] = patch;
		// no enabled head to split at means service is disabled
		if (!ostart)
		{
			for (auto& [pos, op] : ops)
			{
				enabled_[pos] = false;
			}
			disabled_ = true;
			if (on_not_ready_)
			{
				on_not_ready_();
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
			lookup_[current] = tint;
			for (Index pos = NextRingPosition(lookup_.size(), current);
			     !enabled_[pos] && pos != next;
			     pos = NextRingPosition(lookup_.size(), pos))
			{
				lookup_[pos] = tint;
			}
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

		if (disabled_)
		{
			disabled_ = false;
			if (on_ready_)
			{
				on_ready_();
			}
		}
	}

	template<typename IdIter, typename WeightIter>
	Patch UpdateState(IdIter ids_begin, IdIter ids_end, WeightIter weights_begin)
	{
		return state_.Update(ids_begin, ids_end, weights_begin);
	}
	auto Lookup()
	{
		return std::pair{lookup_.cbegin(), disabled_ ? lookup_.cbegin() : lookup_.cend()};
	}
};

namespace
{
void PatchPatch(Patch& p, Patch& pp)
{
	p.operations.merge(pp.operations);
	for (auto& [idx, op] : pp.operations)
	{
		p.operations.at(idx) = op;
	}
	p.enabled_head = pp.enabled_head;
}

}

class Balancer : Logger
{
	using ServiceId = std::uint32_t;
	using PatchBundle = std::unordered_map<ServiceId, Patch>;
	std::unordered_map<ServiceId, Service> services_;
	std::thread updater_;
	std::atomic<bool> need_updater_;
	Exclusive<PatchBundle> patches_;

	void UpdaterSweep()
	{
		auto patches = patches_.apply([this](PatchBundle& patches) {
			//need_updater_.store(false, std::memory_order_release);
			return std::exchange(patches, {});
		});
		bool empty = true;
		for (auto& [sid, patch] : patches)
		{
			empty &= patch.operations.empty();
			services_.at(sid).UpdateLookup(patch);
		}
		if (empty)
		{
			using namespace std::chrono_literals;
			std::this_thread::sleep_for(1ms);
		}
	}

	void StopUpdater()
	{
		need_updater_.store(false, std::memory_order_release);
		if (updater_.joinable())
		{
			updater_.join();
			Debug("Joined lookup updater thread");
		}
	}

	void StartUpdater()
	{
		need_updater_.store(true, std::memory_order_release);
		if (!updater_.joinable())
		{
			updater_ = std::thread([this]() {
				Debug("Started Updater thread");
				while (need_updater_.load(std::memory_order_acquire))
				{
					UpdaterSweep();
				};
				Debug("Ending Updater thread");
			});
		}
	}

public:
	~Balancer()
	{
		StopUpdater();
	}
	template<typename IdIter, typename RealIter, typename WeightIter>
	void AddService(ServiceId id, IdIter ids_begin, IdIter ids_end, RealIter reals_begin, WeightIter weights_begin)
	{
		StopUpdater();
		if (services_.find(id) != services_.end())
		{
			Error("Service already exists");
			return;
		}
		auto oservice = Service::MakeService(ids_begin, ids_end, reals_begin, weights_begin);
		if (!oservice)
		{
			Error("Failed to add service ", id, " to balancer");
			return;
		}
		services_.emplace(id, std::move(oservice.value()));
	}

	void ClearServices()
	{
		StopUpdater();
		services_.clear();
		patches_.apply([](PatchBundle& patches) {
			patches = PatchBundle{};
		});
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

	void RemoveService(ServiceId id)
	{
		if (auto svc = services_.find(id); svc != services_.end())
		{
			StopUpdater();
			svc->second.Disable();
			services_.erase(id);
			patches_.apply([id](PatchBundle& patches){
				patches.erase(id);
			});
			if (!services_.empty())
			{
				StartUpdater();
			}
		}
	}

	template<typename IdIter, typename WeightIter>
	void UpdateWeights(ServiceId id, IdIter ids_begin, IdIter ids_end, WeightIter weights_begin)
	{
		if (auto it = services_.find(id); it != services_.end())
		{
			auto& [sid, service] = *it;

			Patch patch = service.UpdateState(ids_begin, ids_end, weights_begin);
			patches_.apply([&sid, &patch, this](PatchBundle& patches) {
				PatchPatch(patches[sid], patch);
				StartUpdater();
			});
		}
		else
		{
			Error("Service ", id, " not found");
		}
	}
};

} // namespace chash