#include <optional>
#include <vector>

#include <chash.hpp>
namespace test
{

using RealId = chash::DefaultConfig::RealId;
using Weight = chash::DefaultConfig::Weight;

constexpr std::size_t DFLT_MAPPINGS = 100;
constexpr std::size_t DFLT_CELLS = 20;

struct UpdaterInput
{
	std::vector<std::string> reals = {"alpha", "beta", "gamma", "delta"};
	std::vector<RealId> ids = {1, 2, 3, 4};
	std::vector<Weight> weights = {100, 100, 100, 100};
	std::size_t mappings = DFLT_MAPPINGS;
	std::size_t cells = DFLT_CELLS;
	std::size_t lookup_size = chash::WeightUpdater::LookupRequiredSize(ids.size(), DFLT_CELLS);
	std::size_t TotalWeight() const
	{
		return std::accumulate(weights.begin(), weights.end(), 0);
	}
};

std::optional<chash::WeightUpdater> MakeUpdater(const UpdaterInput& input)
{
	std::size_t size = input.lookup_size;
	if (size == 0)
	{
		size = chash::WeightUpdater::LookupRequiredSize(input.ids.size(), input.cells);
	}
	return chash::MakeWeightUpdater(
	        input.reals.data(),
	        input.ids.data(),
	        input.weights.data(),
	        input.ids.size(),
	        input.mappings,
	        input.cells,
	        size);
}

}