#if MOVED
#pragma once
#include <cstdint>
#include <mutex>

namespace chash
{
inline std::size_t ChangeRingPosition(std::size_t ring_size, std::size_t pos, int offset)
{
	return (ring_size + pos + offset) % ring_size;
}

std::size_t RingPosition(std::size_t ring_size, std::size_t pos);

inline std::size_t NextRingPosition(std::size_t ring_size, std::size_t pos)
{
	return ChangeRingPosition(ring_size, pos, 1);
}

std::size_t PrevRingPosition(std::size_t ring_size, std::size_t pos);

template<std::size_t RingSize>
std::size_t NextRingPosition(std::size_t pos)
{
	return (RingSize + pos + 1) % RingSize;
}

inline std::uint8_t PowerOfTwoLowerBound(std::size_t x)
{
	std::uint8_t p{};
	for (std::size_t y = 1; y < x; y <<= 1, ++p)
	{
	}
	return p;
}

template <typename T>
class Exclusive
{
	std::mutex mx_;
	T data_;
public:
	template<typename F>
	auto apply(F f)
	{
		std::lock_guard guard(mx_);
		return f(data_);
	}
};

} // namespace chash
#endif