#pragma once

#include "Character.hpp"

#include <vector>

namespace sv {

class Roster {
public:
    Roster();
    bool setDefinitions(const std::vector<CharacterDefinition>& definitions);
    const CharacterDefinition* definition(CharacterId id) const;
    const std::vector<CharacterDefinition>& definitions() const { return definitions_; }
    std::vector<CharacterId> starterIds() const;

    void reset();
    bool beginNewGame(const std::vector<CharacterId>& selectedStarters);
    bool recruit(CharacterId id);
    bool setActiveParty(const std::vector<CharacterId>& ids);

    CharacterRecord* find(CharacterId id);
    const CharacterRecord* find(CharacterId id) const;
    const std::vector<CharacterRecord>& records() const { return records_; }

    bool damage(CharacterId id, int amount);
    bool heal(CharacterId id, int amount);
    bool restore(const std::vector<CharacterRecord>& records);

private:
    std::vector<CharacterDefinition> definitions_{characterDefinitions()};
    std::vector<CharacterRecord> records_;
};

} // namespace sv
