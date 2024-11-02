#pragma once
#include <cstdint>
#include <vector>

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

template<std::size_t RingSize>
std::size_t NextRingPosition(std::size_t pos)
{
	return (RingSize + pos + 1) % RingSize;
}

