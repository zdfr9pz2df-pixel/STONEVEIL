#include "Character.hpp"
#include "AtomicFile.hpp"
#include <fstream>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <filesystem>

namespace sv {
std::vector<std::string> CharacterCatalogIO::validate(const std::vector<CharacterDefinition>& definitions) {
    std::vector<std::string> errors;
    if (definitions.empty() || definitions.size() > 256) errors.push_back("A project needs 1 to 256 character definitions.");
    std::set<CharacterId> ids;
    std::set<std::string> keys;
    int starters = 0;
    for (const auto& d : definitions) {
        if (!d.id || d.id > static_cast<CharacterId>(std::numeric_limits<int>::max()) || !ids.insert(d.id).second)
            errors.push_back("Character IDs must be unique numbers from 1 to 2147483647.");
        if (d.stableKey.empty() || !keys.insert(d.stableKey).second || d.stableKey.find_first_of(" \t\r\n") != std::string::npos)
            errors.push_back("Character reference keys must be unique and contain no spaces.");
        if (d.name.empty() || d.name.size() > 80 || d.role.empty() || d.role.size() > 80)
            errors.push_back("Characters need a name and role (up to 80 characters).");
        if (d.maxHp < 1 || d.maxHp > 9999 || d.power < 1 || d.power > 999)
            errors.push_back("Character health must be 1-9999 and power 1-999.");
        if (d.summary.size() > 1000 || d.traits.size() > 1000) errors.push_back("Character description/traits are too long.");
        if (d.equipmentTags.size() > 500 || d.startingEquipment.size() > 500 || d.recruitmentText.size() > 1000)
            errors.push_back("Character equipment/recruitment text is too long.");
        if (d.attackId != "attack.melee" || (d.abilityId != "ability.heal" && !d.abilityId.empty()))
            errors.push_back("Choose a supported character attack and ability.");
        if (!d.portraitPath.empty()) {
            const std::filesystem::path path{d.portraitPath};
            bool safe = d.portraitPath.rfind("content/portraits/", 0) == 0 &&
                d.portraitPath.find_first_of(":\\") == std::string::npos && !path.is_absolute();
            for (const auto& part : path) if (part == ".." || part == ".") safe = false;
            if (!safe) errors.push_back("Portraits must use a relative file inside content/portraits.");
        }
        if (d.starter) ++starters;
    }
    if (starters < 1 || starters > 3) errors.push_back("Choose between 1 and 3 starting-party candidates.");
    return errors;
}
bool CharacterCatalogIO::load(const std::string& path, std::vector<CharacterDefinition>& definitions, std::string& error) {
    std::ifstream input(path);
    std::string signature, end;
    int version{}, count{};
    if (!(input >> signature >> version >> count) || signature != "STONEVEIL_CHARACTERS" || version != 1 || count < 1 || count > 256) {
        error = "Invalid character catalog header."; return false;
    }
    std::vector<CharacterDefinition> loaded;
    for (int i = 0; i < count; ++i) {
        CharacterDefinition d; int starter{};
        input >> d.id >> std::quoted(d.stableKey) >> std::quoted(d.name) >> std::quoted(d.role) >>
            std::quoted(d.summary) >> d.maxHp >> d.power >> starter >> std::quoted(d.portraitPath) >>
            std::quoted(d.traits) >> std::quoted(d.attackId) >> std::quoted(d.abilityId);
        int recruitable{};
        input >> std::quoted(d.equipmentTags) >> std::quoted(d.startingEquipment) >> recruitable >> std::quoted(d.recruitmentText);
        if (!input || (starter != 0 && starter != 1) || (recruitable != 0 && recruitable != 1)) { error = "Invalid character record."; return false; }
        d.starter = starter != 0; loaded.push_back(std::move(d));
        loaded.back().recruitable = recruitable != 0;
    }
    if (!(input >> end) || end != "END") { error = "Character catalog is incomplete."; return false; }
    const auto errors = validate(loaded);
    if (!errors.empty()) { error = errors.front(); return false; }
    definitions = std::move(loaded); error.clear(); return true;
}
bool CharacterCatalogIO::save(const std::string& path, const std::vector<CharacterDefinition>& definitions, std::string& error) {
    const auto errors = validate(definitions);
    if (!errors.empty()) { error = errors.front(); return false; }
    std::ostringstream out;
    out << "STONEVEIL_CHARACTERS 1\n" << definitions.size() << '\n';
    for (const auto& d : definitions) out << d.id << ' ' << std::quoted(d.stableKey) << ' ' <<
        std::quoted(d.name) << ' ' << std::quoted(d.role) << ' ' << std::quoted(d.summary) << ' ' <<
        d.maxHp << ' ' << d.power << ' ' << d.starter << ' ' << std::quoted(d.portraitPath) << ' ' <<
        std::quoted(d.traits) << ' ' << std::quoted(d.attackId) << ' ' << std::quoted(d.abilityId) << ' ' <<
        std::quoted(d.equipmentTags) << ' ' << std::quoted(d.startingEquipment) << ' ' << d.recruitable << ' ' <<
        std::quoted(d.recruitmentText) << '\n';
    out << "END\n";
    return writeFileAtomically(path, out.str(), error);
}
}
