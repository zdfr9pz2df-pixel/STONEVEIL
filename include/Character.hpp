#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sv {

using CharacterId = std::uint32_t;
constexpr CharacterId InvalidCharacterId = 0;

namespace character_ids {
constexpr CharacterId Vanguard = 1001;
constexpr CharacterId Ranger = 1002;
constexpr CharacterId Mystic = 1003;
} // namespace character_ids

enum class CharacterStatus : std::uint8_t {
    Unrecruited = 0,
    Reserve = 1,
    Active = 2,
    Dead = 3,
};

struct CharacterDefinition {
    CharacterId id{InvalidCharacterId};
    std::string stableKey;
    std::string name;
    std::string role;
    std::string summary;
    int maxHp{};
    int power{};
    bool starter{false};
    std::string portraitPath;
    std::string traits;
    std::string attackId{"attack.melee"};
    std::string abilityId{"ability.heal"};
    std::string equipmentTags{"weapon,armor"};
    std::string startingEquipment;
    bool recruitable{true};
    std::string recruitmentText;
};

struct CharacterRecord {
    CharacterId id{InvalidCharacterId};
    int hp{};
    int xp{};
    CharacterStatus status{CharacterStatus::Unrecruited};

    bool alive() const { return hp > 0 && status != CharacterStatus::Dead; }
};

const std::vector<CharacterDefinition>& characterDefinitions();
class CharacterCatalogIO {
public:
    static std::vector<std::string> validate(const std::vector<CharacterDefinition>& definitions);
    static bool load(const std::string& path, std::vector<CharacterDefinition>& definitions, std::string& error);
    static bool save(const std::string& path, const std::vector<CharacterDefinition>& definitions, std::string& error);
};
const CharacterDefinition* findCharacterDefinition(CharacterId id);
std::vector<CharacterId> starterCharacterIds();

} // namespace sv
