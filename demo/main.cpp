#include <chash/service.hpp>

int main()
{
	std::vector<chash::RealId> buf;
	buf.resize(100);

	std::vector<std::uint32_t> ids = {1, 2, 3, 4, 5};
	chash::Service service =
	        chash::Service::Make(
	                buf.data(),
	                chash::LookupRequiredSize(ids.size(), chash::DEFAULT_SEGMENTS_PER_WEIGHT),
	                ids.begin(),
	                ids.end(),
	                ids.begin(),
	                ids.begin())
	                .value();
	return 0;
}