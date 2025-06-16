#pragma once
#include <cstdint>
#include <mutex>

namespace chash
{
std::size_t ChangeRingPosition(std::size_t ring_size, std::size_t pos, int offset);

std::size_t RingPosition(std::size_t ring_size, std::size_t pos);

std::size_t NextRingPosition(std::size_t ring_size, std::size_t pos);

std::size_t PrevRingPosition(std::size_t ring_size, std::size_t pos);

template<std::size_t RingSize>
std::size_t NextRingPosition(std::size_t pos)
{
	return (RingSize + pos + 1) % RingSize;
}

std::uint8_t PowerOfTwoLowerBound(std::size_t x);

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
