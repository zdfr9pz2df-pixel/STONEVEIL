#include "Campaign.hpp"
#include "AtomicFile.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>

namespace sv {
namespace {
bool safeLevelPath(const std::string& path) {
    return path.rfind("content/levels/", 0) == 0 && path.size() > 4 &&
        path.substr(path.size() - 4) == ".svl" && path.find("..") == std::string::npos &&
        path.find('\\') == std::string::npos && path.find(':') == std::string::npos;
}
}

bool CampaignIO::load(const std::string& path, CampaignDefinition& campaign, std::string& error) {
    std::ifstream input(path);
    std::string signature;
    int version{};
    CampaignDefinition loaded;
    std::size_t count{};
    std::string token;
    if (!input || !(input >> signature >> version) || signature != "STONEVEIL_CAMPAIGN" || version != 1 ||
        !(input >> token) || token != "ID" || !(input >> std::quoted(loaded.id)) ||
        !(input >> token) || token != "NAME" || !(input >> std::quoted(loaded.name)) ||
        !(input >> token) || token != "START" || !(input >> std::quoted(loaded.startingLevelId)) ||
        !(input >> token) || token != "LEVELS" || !(input >> count) || count > 256) {
        error = "Malformed or unsupported campaign registry.";
        return false;
    }
    for (std::size_t i = 0; i < count; ++i) {
        CampaignLevelEntry entry;
        if (!(input >> std::quoted(entry.id) >> std::quoted(entry.name) >> std::quoted(entry.path))) {
            error = "Malformed campaign level entry.";
            return false;
        }
        loaded.levels.push_back(std::move(entry));
    }
    if (!(input >> token) || token != "END") {
        error = "Campaign registry is missing END.";
        return false;
    }
    const auto errors = validate(loaded);
    if (!errors.empty()) {
        error = errors.front();
        return false;
    }
    campaign = std::move(loaded);
    error.clear();
    return true;
}

bool CampaignIO::save(const std::string& path, const CampaignDefinition& campaign, std::string& error) {
    const auto errors = validate(campaign);
    if (!errors.empty()) {
        error = errors.front();
        return false;
    }
    std::ostringstream output;
    if (!output) {
        error = "Could not write campaign registry: " + path;
        return false;
    }
    output << "STONEVEIL_CAMPAIGN 1\n";
    output << "ID " << std::quoted(campaign.id) << '\n';
    output << "NAME " << std::quoted(campaign.name) << '\n';
    output << "START " << std::quoted(campaign.startingLevelId) << '\n';
    output << "LEVELS " << campaign.levels.size() << '\n';
    for (const auto& level : campaign.levels) {
        output << std::quoted(level.id) << ' ' << std::quoted(level.name) << ' ' << std::quoted(level.path) << '\n';
    }
    output << "END\n";
    if (!output) {
        error = "Failed while writing campaign registry.";
        return false;
    }
    return writeFileAtomically(path, output.str(), error);
}

std::vector<std::string> CampaignIO::validate(const CampaignDefinition& campaign) {
    std::vector<std::string> errors;
    if (campaign.id.empty()) errors.push_back("Campaign ID cannot be empty.");
    if (campaign.name.empty()) errors.push_back("Campaign name cannot be empty.");
    if (campaign.levels.empty()) errors.push_back("Campaign must contain at least one level.");
    std::set<std::string> ids;
    bool foundStart = false;
    for (const auto& level : campaign.levels) {
        if (level.id.empty() || level.name.empty()) errors.push_back("Campaign levels need an ID and name.");
        if (!ids.insert(level.id).second) errors.push_back("Campaign level IDs must be unique.");
        if (!safeLevelPath(level.path)) errors.push_back("Campaign levels must use safe content/levels/*.svl paths.");
        if (level.id == campaign.startingLevelId) foundStart = true;
    }
    if (!foundStart) errors.push_back("Campaign start level must reference a registered level.");
    return errors;
}

} // namespace sv
