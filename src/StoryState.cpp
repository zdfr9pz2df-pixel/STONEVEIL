#include "StoryState.hpp"

#include <cctype>
#include <utility>

namespace sv {

bool StoryState::validKey(const std::string& key) {
    if (key.empty() || key.size() > 120) return false;
    for (const unsigned char character : key) {
        if (!(std::isalnum(character) || character == '.' || character == '_' || character == '-')) return false;
    }
    return true;
}

bool StoryState::set(const std::string& key, bool value) {
    if (!validKey(key)) return false;
    facts_[key] = value;
    return true;
}

bool StoryState::value(const std::string& key) const {
    const auto found = facts_.find(key);
    return found != facts_.end() && found->second;
}

bool StoryState::contains(const std::string& key) const {
    return facts_.find(key) != facts_.end();
}

bool StoryState::replace(std::map<std::string, bool> facts) {
    if (facts.size() > 4096) return false;
    for (const auto& fact : facts) if (!validKey(fact.first)) return false;
    facts_ = std::move(facts);
    return true;
}

} // namespace sv
