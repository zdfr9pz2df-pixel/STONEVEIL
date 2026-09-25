#include "Party.hpp"

#include <algorithm>

namespace sv {

Party::Party(std::size_t capacity) {
    if (!setCapacity(capacity)) capacity_ = InitialCapacity;
}

bool Party::contains(CharacterId id) const {
    return std::find(members_.begin(), members_.end(), id) != members_.end();
}

bool Party::add(CharacterId id) {
    if (id == InvalidCharacterId || full() || contains(id)) return false;
    members_.push_back(id);
    return true;
}

bool Party::remove(CharacterId id) {
    const auto it = std::find(members_.begin(), members_.end(), id);
    if (it == members_.end()) return false;
    members_.erase(it);
    return true;
}

bool Party::setMembers(const std::vector<CharacterId>& ids) {
    if (ids.size() > capacity_) return false;
    std::vector<CharacterId> validated;
    validated.reserve(ids.size());
    for (const auto id : ids) {
        if (id == InvalidCharacterId || std::find(validated.begin(), validated.end(), id) != validated.end()) return false;
        validated.push_back(id);
    }
    members_ = std::move(validated);
    return true;
}

bool Party::setCapacity(std::size_t capacity) {
    if (capacity == 0 || capacity > MaximumCapacity || capacity < members_.size()) return false;
    capacity_ = capacity;
    return true;
}

bool Party::increaseCapacity(std::size_t amount) {
    if (amount == 0 || amount > MaximumCapacity - capacity_) return false;
    return setCapacity(capacity_ + amount);
}

void Party::reset() {
    members_.clear();
    capacity_ = InitialCapacity;
}

} // namespace sv
