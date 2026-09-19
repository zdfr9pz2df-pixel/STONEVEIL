#include "Roster.hpp"

#include "Party.hpp"

#include <algorithm>
#include <unordered_set>

namespace sv {

Roster::Roster() {
    reset();
}
bool Roster::setDefinitions(const std::vector<CharacterDefinition>& definitions) {
    if (!CharacterCatalogIO::validate(definitions).empty()) return false;
    definitions_ = definitions;
    reset();
    return true;
}
const CharacterDefinition* Roster::definition(CharacterId id) const {
    const auto it = std::find_if(definitions_.begin(), definitions_.end(), [id](const auto& d) { return d.id == id; });
    return it == definitions_.end() ? nullptr : &*it;
}
std::vector<CharacterId> Roster::starterIds() const {
    std::vector<CharacterId> ids;
    for (const auto& d : definitions_) if (d.starter) ids.push_back(d.id);
    return ids;
}

void Roster::reset() {
    records_.clear();
    records_.reserve(definitions_.size());
    for (const auto& definition : definitions_) {
        records_.push_back({definition.id, definition.maxHp, 0, CharacterStatus::Unrecruited});
    }
}

bool Roster::beginNewGame(const std::vector<CharacterId>& selectedStarters) {
    if (selectedStarters.empty() || selectedStarters.size() > Party::InitialCapacity) return false;

    std::unordered_set<CharacterId> selected;
    for (const auto id : selectedStarters) {
        const auto* definition = this->definition(id);
        if (definition == nullptr || !definition->starter || !selected.insert(id).second) return false;
    }

    reset();
    for (auto& record : records_) {
        const auto* definition = this->definition(record.id);
        if (definition != nullptr && definition->starter) record.status = CharacterStatus::Reserve;
    }
    return setActiveParty(selectedStarters);
}

bool Roster::recruit(CharacterId id) {
    auto* record = find(id);
    const auto* definition = this->definition(id);
    if (record == nullptr || definition == nullptr || record->status != CharacterStatus::Unrecruited) return false;
    record->hp = definition->maxHp;
    record->status = CharacterStatus::Reserve;
    return true;
}

bool Roster::setActiveParty(const std::vector<CharacterId>& ids) {
    if (ids.size() > Party::MaximumCapacity) return false;
    std::unordered_set<CharacterId> selected;
    for (const auto id : ids) {
        const auto* record = find(id);
        if (record == nullptr || !record->alive() || record->status == CharacterStatus::Unrecruited || !selected.insert(id).second) {
            return false;
        }
    }

    for (auto& record : records_) {
        if (record.status == CharacterStatus::Active) record.status = CharacterStatus::Reserve;
    }
    for (const auto id : ids) find(id)->status = CharacterStatus::Active;
    return true;
}

CharacterRecord* Roster::find(CharacterId id) {
    const auto it = std::find_if(records_.begin(), records_.end(), [id](const CharacterRecord& record) {
        return record.id == id;
    });
    return it == records_.end() ? nullptr : &*it;
}

const CharacterRecord* Roster::find(CharacterId id) const {
    const auto it = std::find_if(records_.begin(), records_.end(), [id](const CharacterRecord& record) {
        return record.id == id;
    });
    return it == records_.end() ? nullptr : &*it;
}

bool Roster::damage(CharacterId id, int amount) {
    auto* record = find(id);
    if (record == nullptr || !record->alive() || amount <= 0) return false;
    record->hp = std::max(0, record->hp - amount);
    if (record->hp == 0) {
        record->status = CharacterStatus::Dead;
        return true;
    }
    return false;
}

bool Roster::heal(CharacterId id, int amount) {
    auto* record = find(id);
    const auto* definition = this->definition(id);
    if (record == nullptr || definition == nullptr || !record->alive() || amount <= 0 || record->hp >= definition->maxHp) return false;
    record->hp = std::min(definition->maxHp, record->hp + amount);
    return true;
}

bool Roster::restore(const std::vector<CharacterRecord>& records) {
    if (records.size() > definitions_.size()) return false;
    std::unordered_set<CharacterId> seen;
    std::size_t activeCount = 0;
    for (const auto& record : records) {
        const auto* definition = this->definition(record.id);
        if (definition == nullptr || !seen.insert(record.id).second || record.hp < 0 || record.hp > definition->maxHp) return false;
        if ((record.hp == 0) != (record.status == CharacterStatus::Dead)) return false;
        if (record.xp < 0 || record.status < CharacterStatus::Unrecruited || record.status > CharacterStatus::Dead) return false;
        if (record.status == CharacterStatus::Active && ++activeCount > Party::MaximumCapacity) return false;
    }
    reset();
    for (const auto& saved : records) *find(saved.id) = saved;
    return true;
}

} // namespace sv
