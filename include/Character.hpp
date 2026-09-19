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
};

struct CharacterRecord {
    CharacterId id{InvalidCharacterId};
    int hp{};
    int xp{};
    CharacterStatus status{CharacterStatus::Unrecruited};

    bool alive() const { return hp > 0 && status != CharacterStatus::Dead; }
};

const std::vector<CharacterDefinition>& characterDefinitions();
const CharacterDefinition* findCharacterDefinition(CharacterId id);
std::vector<CharacterId> starterCharacterIds();

} // namespace sv
