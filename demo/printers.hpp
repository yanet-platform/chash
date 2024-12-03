#pragma once
#include <iostream>
#include <map>
#include <sstream>
#include <tuple>
#include <unordered_map>
#include <vector>

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

template<typename... Args>
std::string Parenthesize(Args&&... args)
{
	std::stringstream ss;
	ss << '(';
	(ss << ... << args);
	ss << ')';
	return ss.str();
}

template<typename T>
struct Printer
{
	static void Print(const T& t) { std::cout << t; }
};

template<typename U, typename V>
struct Printer<std::pair<U, V>>
{
	static void Print(const std::pair<U, V>& p)
	{
		Embrace emb;
		Printer<std::decay_t<U>>::Print(p.first);
		std::cout << ", ";
		Printer<std::decay_t<V>>::Print(p.second);
	}
};

template<typename C>
void PrintContainer(const C& c)
{
	Embrace emb;
	bool first{true};
	for (auto& e : c)
	{
		if (!first)
		{
			std::cout << ", ";
		}
		else
		{
			first = false;
		}
		Printer<std::decay_t<decltype(e)>>::Print(e);
	}
}

template<typename K, typename V>
struct Printer<std::map<K, V>>
{
	static void Print(const std::map<K, V>& map) { PrintContainer(map); }
};

template<typename T>
struct Printer<std::vector<T>>
{
	static void Print(const std::vector<T>& vec) { PrintContainer(vec); }
};

template<typename K, typename V>
struct Printer<std::unordered_map<K, V>>
{
	static void Print(const std::unordered_map<K,V>& uno) { PrintContainer(uno); }
};

template <typename T>
void Print(const T& t)
{
	Printer<std::decay_t<T>>::Print(t);
}