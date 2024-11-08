#pragma once
#include <map>
#include <tuple>
#include <unordered_map>
#include <vector>
#include <sstream>

class Embrace
{
	std::ostream& os_ = std::cout;

public:
	Embrace()
	{
		os_ << '{';
	}
	~Embrace()
	{
		os_ << '}';
	}
};

template<typename...Args>
std::string Parenthesize(Args&&...args)
{
	std::stringstream ss;
	ss << '(';
	(ss << ... << args);
	ss << ')';
	return ss.str();
}

template<typename T>
void Print(const T& x)
{
	std::cout << x;
}

template<typename T, typename U>
void Print(const std::pair<T, U>& p)
{
	Embrace r;
	std::cout << p.first << ", " << p.second;
}

template<typename T>
void Print(const typename std::pair<T, std::uint8_t>& val)
{
	Embrace r;
	std::cout << val.first << ", " << +val.second;
}

template<typename C>
void PrintContainer(const C& c)
{
	Embrace r;
	if (!c.empty())
	{
		auto it = c.begin();
		Print(*it);
		++it;
		for (; it != c.end(); ++it)
		{
			std::cout << ", ";
			Print(*it);
		}
	}
}

template<typename T>
void Print(const std::vector<T>& vec)
{
	PrintContainer(vec);
}

template<typename K, typename V>
void Print(const std::map<K, V>& vec)
{
	PrintContainer(vec);
}

template<typename K, typename V>
void Print(const std::unordered_map<K, V>& vec)
{
	PrintContainer(vec);
}

std::ostream& operator<<(std::ostream& os, chash::RealConfig cfg)
{
	os << "{" << cfg.id << ", " << static_cast<int>(cfg.weight) << "}";
	return os;
}