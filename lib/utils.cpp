#if MOVED
#include "utils.hpp"

namespace chash
{

	std::size_t RingPosition(std::size_t ring_size, std::size_t pos)
{
	return ChangeRingPosition(ring_size, pos, 0);
}

std::size_t PrevRingPosition(std::size_t ring_size, std::size_t pos)
{
	return ChangeRingPosition(ring_size, pos, -1);
}

} // namespace chash
#endif