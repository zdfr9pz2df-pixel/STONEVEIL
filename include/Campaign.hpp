#pragma once

#include <string>
#include <vector>

namespace sv {

struct CampaignLevelEntry {
    std::string id;
    std::string name;
    std::string path;
};

struct CampaignDefinition {
    std::string id;
    std::string name;
    std::string startingLevelId;
    std::vector<CampaignLevelEntry> levels;
};

class CampaignIO {
public:
    static bool load(const std::string& path, CampaignDefinition& campaign, std::string& error);
    static bool save(const std::string& path, const CampaignDefinition& campaign, std::string& error);
    static std::vector<std::string> validate(const CampaignDefinition& campaign);
};

} // namespace sv
