#include "hash.hpp"

namespace chash {

IdHash CalcHash(const std::string& data, IdHash prev)
{
	return crc32_fast(static_cast<const void*>(data.c_str()), data.size() * sizeof(std::string::value_type), prev);
}

} // namespace chash