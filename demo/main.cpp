#include <vector>

#include "balancer.hpp"
#include "common.hpp"

int main()
{

	chash::Balancer balancer;

	std::vector<std::uint32_t> ids{1, 2, 3, 4, 5, 6};
	std::vector<std::uint32_t> weights{1, 1, 1, 1, 1, 1};

	balancer.AddService(1, ids.begin(), ids.end(), ids.begin(), weights.begin());

	weights[4] = 100;
	balancer.UpdateWeights(1, ids.begin(), ids.end(), weights.begin());
	balancer.UpdateLookups();

	{
		auto [l, size] = balancer.Lookup(1);

		std::unordered_map<std::uint32_t, std::uint32_t> cnt;
		for (std::size_t i = 0; i < size; ++i)
		{
			++cnt[l[i]];
		}

		for (auto [id, c] : cnt)
		{
			std::cout << id << ": " << c << ", ";
		}
		std::cout << std::endl;
	}

	balancer.AdjustLookups();

	{
		auto [l, size] = balancer.Lookup(1);

		std::unordered_map<std::uint32_t, std::uint32_t> cnt;
		for (std::size_t i = 0; i < size; ++i)
		{
			++cnt[l[i]];
		}

		for (auto [id, c] : cnt)
		{
			std::cout << id << ": " << c << ", ";
		}
		std::cout << std::endl;
	}

	return 0;
}