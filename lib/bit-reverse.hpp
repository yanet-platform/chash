#pragma once
#include <cstdint>
#include <limits>

inline constexpr std::uint8_t BitReverse(std::uint8_t x)
{
	x = ((x & 0x55) << 1) | ((x & 0xAA) >> 1);
	x = ((x & 0x33) << 2) | ((x & 0xCC) >> 2);
	x = ((x & 0x0F) << 4) | ((x & 0xF0) >> 4);
	return x;
}

inline constexpr std::uint16_t BitReverse(std::uint16_t x)
{
	x = ((x & 0x5555) << 1) | ((x & 0xAAAA) >> 1);
	x = ((x & 0x3333) << 2) | ((x & 0xCCCC) >> 2);
	x = ((x & 0x0F0F) << 4) | ((x & 0xF0F0) >> 4);
	x = ((x & 0x00FF) << 8) | ((x & 0xFF00) >> 8);
	return x;
}

inline constexpr std::uint32_t BitReverse(std::uint32_t x)
{
	x = ((x & 0x55555555) << 1) | ((x & 0xAAAAAAAA) >> 1);
	x = ((x & 0x33333333) << 2) | ((x & 0xCCCCCCCC) >> 2);
	x = ((x & 0x0F0F0F0F) << 4) | ((x & 0xF0F0F0F0) >> 4);
	x = ((x & 0x00FF00FF) << 8) | ((x & 0xFF00FF00) >> 8);
	x = ((x & 0x0000FFFF) << 16) | ((x & 0xFFFF0000) >> 16);
	return x;
}

template<uint8_t Bits>
inline constexpr std::uint32_t FractionalBitReverse(std::uint32_t x)
{
	auto y = BitReverse(x) >> (32 - Bits);
	return y;
}

template<uint8_t Bits>
class BitReverseSequence
{
	using size_t = std::size_t;
	class Iterator
	{
		size_t idx_;
		bool end_ = false;

	public:
		Iterator(size_t idx) :
		        idx_{idx} {}

		size_t operator*()
		{
			return FractionalBitReverse<Bits>(idx_);
		}

		Iterator& operator++()
		{
			if (idx_ == std::numeric_limits<size_t>::max() >> (std::numeric_limits<size_t>::digits - Bits))
			{
				end_ = true;
			}
			else
			{
				idx_++;
			}
			return *this;
		}

		bool operator==(const Iterator& other)
		{
			return (end_ && other.end_) || (!end_ && !other.end_ && idx_ == other.idx_);
		}

		bool operator!=(const Iterator& other)
		{
			return !(*this == other);
		}

		static Iterator end()
		{
			Iterator i{0};
			i.end_ = true;
			return i;
		}
	};

public:
	Iterator begin()
	{
		return Iterator(0);
	}
	Iterator end()
	{
		return Iterator::end();
	}
};