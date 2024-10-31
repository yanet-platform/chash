#pragma once
#include <cstdint>
#include <vector>

std::size_t NextInRing(std::size_t ring_size, std::size_t pos)
{
	return (pos + ring_size + 1) % ring_size;
}

class RingIterator
{
	std::size_t index_ = 0;
	std::size_t size_ = 1;

	void Next()
	{
		if (index_ == size_ - 1)
		{
			index_ = 0;
		}
		else
		{
			++index_;
		}
	}

	void Prev()
	{
		if (index_ == 0)
		{
			index_ = size_ - 1;
		}
		else
		{
			--index_;
		}
	}

	RingIterator& Advance(int i)
	{
		index_ = (size_ + index_ + i) % size_;
		return *this;
	}

public:
	RingIterator(std::size_t size, std::size_t index = 0) :
	        index_{index},
	        size_{size}
	{
	}

	operator std::size_t()
	{
		return index_;
	}

	RingIterator operator++(int)
	{
		RingIterator result{*this};
		Next();
		return result;
	}

	RingIterator& operator++()
	{
		Next();
		return *this;
	}

	RingIterator operator--(int)
	{
		RingIterator result{*this};
		Prev();
		return *this;
	}

	RingIterator& operator--()
	{
		Prev();
		return *this;
	}

	RingIterator operator-(int i)
	{
		RingIterator res{*this};
		res.Advance(-i);
		return res;
	}

	RingIterator operator+(int i)
	{
		RingIterator res{*this};
		res.Advance(i);
		return res;
	}
};