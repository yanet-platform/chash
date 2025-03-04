#pragma once
#include <cstdint>
#include <string>

#include "../3rdparty/Crc32.h"

namespace chash {

using Salt = std::uint32_t; // Salt is what differentiates one Unweighted from another
using IdHash = std::uint32_t; // IdHash = f(Real, Salt)

template<typename T>
IdHash CalcHash(const T& data, IdHash prev)
{
	return crc32_fast(static_cast<const void*>(&data), sizeof(data), prev);
}

IdHash CalcHash(const std::string& data, IdHash prev);

}