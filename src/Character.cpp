#include "Character.hpp"

#include <algorithm>

namespace sv {

const std::vector<CharacterDefinition>& characterDefinitions() {
    static const std::vector<CharacterDefinition> definitions = {
        {character_ids::Vanguard, "starter.vanguard", "Vanguard", "Guardian",
         "Hardiest front-line fighter.", 42, 9, true},
        {character_ids::Ranger, "starter.ranger", "Ranger", "Pathfinder",
         "Balanced explorer and striker.", 32, 7, true},
        {character_ids::Mystic, "starter.mystic", "Mystic", "Arcanist",
         "Fragile future spell specialist.", 27, 6, true},
    };
    return definitions;
}

const CharacterDefinition* findCharacterDefinition(CharacterId id) {
    const auto& definitions = characterDefinitions();
    const auto it = std::find_if(definitions.begin(), definitions.end(), [id](const CharacterDefinition& definition) {
        return definition.id == id;
    });
    return it == definitions.end() ? nullptr : &*it;
}

std::vector<CharacterId> starterCharacterIds() {
    std::vector<CharacterId> ids;
    for (const auto& definition : characterDefinitions()) {
        if (definition.starter) ids.push_back(definition.id);
    }
    return ids;
}

} // namespace sv
