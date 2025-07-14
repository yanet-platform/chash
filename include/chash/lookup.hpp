#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>

#include <chash/patch.hpp>

namespace chash
{

class Lookup
{
	std::vector<bool> enabled_;
	std::unordered_map<RealId, Index> stat_;
	RealId* data_;

	void EnableDirty(Index idx, RealId id);
	void DisableDirty(Index idx);
	void EnableDirty(std::vector<Patch::on_t>::const_iterator begin,
	                 std::vector<Patch::on_t>::const_iterator end);
	void DisableDirty(std::vector<Index>::const_iterator begin,
	                  std::vector<Index>::const_iterator end);
	void FixSeam();
public:
	Lookup(RealId* buf, Index size);
	const RealId* data() const;
	Index size() const;
	void Update(Patch& patch);
	void Enable(Index pos, RealId id);
	void Disable(Index pos);
	void DisablingUpdate(const std::vector<Index>& off);
	const std::unordered_map<RealId, Index>& Stats() const;
};

} // namespace chash

#include <chash/lookup.ipp>