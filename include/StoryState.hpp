#pragma once

#include <map>
#include <string>
#include <vector>

namespace sv {

// Campaign-wide named narrative facts. Keys are stable authored identities;
// display text and level-local event history live elsewhere.
class StoryState {
public:
    bool set(const std::string& key, bool value = true);
    bool value(const std::string& key) const;
    bool contains(const std::string& key) const;
    bool replace(std::map<std::string, bool> facts);
    void clear() { facts_.clear(); }
    const std::map<std::string, bool>& facts() const { return facts_; }

    static bool validKey(const std::string& key);

private:
    std::map<std::string, bool> facts_;
};

} // namespace sv
