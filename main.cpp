#include <iostream>

#include "balancer.h"

template <typename T, typename U>
std::ostream& operator<<(std::ostream& out, const typename std::pair<T,U>& val)
{
	out << "{" << val.first << ", " << val.second << '}';
	return out;
}

template <typename T>
std::ostream& operator<<(std::ostream& out, const std::vector<T>& v)
{
	out << "{ ";
	if (!v.empty())
	{
		out << *(v.begin());
	}
	if (v.size() > 1)
	{
		for (auto e = std::next(v.begin()), end = v.end(); e != end ; ++e)
		{
			out << ", " << *e;
		}
	}
	out << " }";
	return out;
}

template <typename T, typename U>
std::ostream& operator<<(std::ostream& out, const std::map<T, U>& m)
{
	out << "{ ";
	if (!m.empty())
	{
		out << *(m.begin());
	}
	if (m.size() > 1)
	{
		for (auto e = std::next(m.begin()), end = m.end(); e != end ; ++e)
		{
			out << ", " << *e;
		}
	}
	out << " }";
	return out;
}

int main(void)
{
	std::cout << "Demo start." << std::endl;
	std::vector<std::pair<std::uint16_t, std::uint16_t>> reals = {
		{1, 10}, {3, 20}, {5, 1}, {7, 25}
	};
	std::cout << reals << std::endl;
	balancer::Service svc(reals);
	std::cout << "layout:" << svc.Reference() << std::endl;

	std::cout << "lookup:" << svc.Layout() << std::endl;

	return 0;
}
