#include "utils.hpp"

namespace chash
{
std::size_t ChangeRingPosition(std::size_t ring_size, std::size_t pos, int offset)
{
	return (ring_size + pos + offset) % ring_size;
}

std::size_t RingPosition(std::size_t ring_size, std::size_t pos)
{
	return ChangeRingPosition(ring_size, pos, 0);
}

std::size_t NextRingPosition(std::size_t ring_size, std::size_t pos)
{
	return ChangeRingPosition(ring_size, pos, 1);
}

std::size_t PrevRingPosition(std::size_t ring_size, std::size_t pos)
{
	return ChangeRingPosition(ring_size, pos, -1);
}

std::uint8_t PowerOfTwoLowerBound(std::size_t x)
{
	std::uint8_t p{};
	for (std::size_t y = 1; y < x; y <<= 1, ++p)
	{
	}
	return p;
}

} // namespace chash
