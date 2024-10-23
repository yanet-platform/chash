#pragma once

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
