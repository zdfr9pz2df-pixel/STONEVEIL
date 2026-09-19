#pragma once

#include "Campaign.hpp"
#include <string>
#include <vector>

namespace sv {
enum class IssueSeverity { Error, Warning };
struct ProjectIssue { IssueSeverity severity; std::string message; };

class ProjectDocument {
public:
    bool open(const std::string& projectFile, std::string& error);
    bool create(const std::string& projectFile, const std::string& assetRoot, std::string& error);
    bool save(std::string& error) const;
    bool registerLevel(const std::string& levelFile, std::string& error);
    bool setStartingLevel(const std::string& id, std::string& error);
    std::vector<ProjectIssue> validate() const;
    bool exportWindowsGame(const std::string& executable, const std::string& destination, std::string& error) const;

    bool isOpen() const { return !path_.empty(); }
    const std::string& path() const { return path_; }
    const std::string& root() const { return root_; }
    const CampaignDefinition& campaign() const { return campaign_; }
    std::string levelPath(const std::string& id) const;
    static bool resolveInside(const std::string& root, const std::string& relative, std::string& resolved);

private:
    std::string path_;
    std::string root_;
    std::string campaignPath_{"content/campaigns/main.campaign"};
    CampaignDefinition campaign_;
};
}
