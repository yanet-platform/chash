#include <chash/service.hpp>

int main()
{
	std::vector<std::uint32_t> buf;
	buf.resize(100);

	std::vector<std::uint32_t> ids = {1, 2, 3, 4, 5};
	chash::Service<std::uint32_t> service =
	        chash::Service<std::uint32_t>::Make(
	                buf.data(),
	                chash::LookupRequiredSize(ids.size(), chash::DEFAULT_SEGMENTS_PER_WEIGHT),
	                ids.begin(),
	                ids.end(),
	                ids.begin(),
	                ids.begin())
	                .value();
	return 0;
}