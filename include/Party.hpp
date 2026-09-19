#pragma once

#include "Character.hpp"

#include <cstddef>
#include <vector>

namespace sv {

class Party {
public:
    static constexpr std::size_t InitialCapacity = 3;
    static constexpr std::size_t MaximumCapacity = 3;

    explicit Party(std::size_t capacity = InitialCapacity);

    const std::vector<CharacterId>& members() const { return members_; }
    std::size_t size() const { return members_.size(); }
    std::size_t capacity() const { return capacity_; }
    bool empty() const { return members_.empty(); }
    bool full() const { return members_.size() >= capacity_; }

    bool contains(CharacterId id) const;
    bool add(CharacterId id);
    bool remove(CharacterId id);
    bool setMembers(const std::vector<CharacterId>& ids);
    bool setCapacity(std::size_t capacity);
    bool increaseCapacity(std::size_t amount = 1);
    void clear() { members_.clear(); }
    void reset();

private:
    std::size_t capacity_{InitialCapacity};
    std::vector<CharacterId> members_;
};

} // namespace sv
