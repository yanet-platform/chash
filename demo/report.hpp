#include <iostream>
#include <map>
#include <vector>

#include "printers.hpp"

namespace report
{

void Weights(const std::vector<std::uint32_t>& lookup)
{
	std::map<std::uint32_t, std::size_t> m;
	for (const auto& id : lookup)
	{
		++m[id];
	}
	Print(m);
	std::cout << std::endl;
}

void Clumps(const std::vector<std::uint32_t>& lookup)
{
	std::map<std::uint32_t, std::map<std::size_t, std::size_t>> m;
	std::size_t start = 1;
	for (; lookup[start] == lookup[start - 1] && start < lookup.size(); ++start)
	{
	}
	if (start == lookup.size())
	{
		m[lookup[0]][lookup.size()] = 1;
		Print(m);
		std::cout << std::endl;
		return;
	}
	std::uint32_t id = lookup[start];
	std::size_t l{};
	for (std::size_t i = 0; i < lookup.size(); ++i)
	{
		auto curr = lookup[(start + i) % lookup.size()];
		if (id == curr)
		{
			++l;
		}
		else
		{
			++m[id][l];
			id = curr;
			l = 1;
		}
	}
	Print(m);
	std::cout << std::endl;
}

} // namespace report