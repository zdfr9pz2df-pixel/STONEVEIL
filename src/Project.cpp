#include "Project.hpp"
#include "AtomicFile.hpp"
#include "LevelDocument.hpp"
#include "LevelIO.hpp"
#include "Material.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>

namespace sv {
namespace fs = std::filesystem;
namespace {
bool copyFile(const fs::path& from, const fs::path& to, std::string& error) {
    std::error_code ec;
    fs::create_directories(to.parent_path(), ec);
    if (!ec) fs::copy_file(from, to, fs::copy_options::none, ec);
    if (ec) { error = "Could not copy " + from.string() + ": " + ec.message(); return false; }
    return true;
}
std::string manifest(const CampaignDefinition& campaign, const std::string& campaignPath) {
    std::ostringstream out;
    out << "STONEVEIL_PROJECT 1\nID " << std::quoted(campaign.id) << "\nNAME " <<
        std::quoted(campaign.name) << "\nCAMPAIGN " << std::quoted(campaignPath) << "\nEND\n";
    return out.str();
}
}

bool ProjectDocument::resolveInside(const std::string& root, const std::string& relative, std::string& resolved) {
    if (root.empty() || relative.empty() || relative.find('\\') != std::string::npos ||
        relative.find(':') != std::string::npos || fs::path{relative}.is_absolute()) return false;
    for (const auto& part : fs::path{relative}) if (part == ".." || part == ".") return false;
    std::error_code ec;
    const auto base = fs::weakly_canonical(root, ec);
    if (ec) return false;
    const auto candidate = fs::weakly_canonical(base / relative, ec);
    if (ec) return false;
    auto b = base.begin();
    auto c = candidate.begin();
    for (; b != base.end(); ++b, ++c) if (c == candidate.end() || *b != *c) return false;
    if (c == candidate.end()) return false;
    resolved = candidate.string();
    return true;
}

bool ProjectDocument::open(const std::string& projectFile, std::string& error) {
    std::ifstream input(projectFile);
    std::string signature, token, id, name, campaignPath;
    int version{};
    if (!(input >> signature >> version) || signature != "STONEVEIL_PROJECT" || version != 1 ||
        !(input >> token) || token != "ID" || !(input >> std::quoted(id)) ||
        !(input >> token) || token != "NAME" || !(input >> std::quoted(name)) ||
        !(input >> token) || token != "CAMPAIGN" || !(input >> std::quoted(campaignPath)) ||
        !(input >> token) || token != "END") {
        error = "This is not a supported STONEVEIL project file."; return false;
    }
    std::error_code ec;
    ProjectDocument loaded;
    loaded.path_ = fs::absolute(projectFile, ec).lexically_normal().string();
    if (ec) { error = ec.message(); return false; }
    loaded.root_ = fs::path{loaded.path_}.parent_path().string();
    loaded.campaignPath_ = campaignPath;
    std::string registry;
    if (!resolveInside(loaded.root_, campaignPath, registry)) {
        error = "The campaign file must be inside this project folder."; return false;
    }
    if (!CampaignIO::load(registry, loaded.campaign_, error)) return false;
    if (id != loaded.campaign_.id || name != loaded.campaign_.name) {
        error = "Project identity does not match its campaign registry."; return false;
    }
    for (const auto& issue : loaded.validate()) {
        if (issue.severity == IssueSeverity::Error) { error = issue.message; return false; }
    }
    *this = std::move(loaded);
    error.clear();
    return true;
}

bool ProjectDocument::create(const std::string& projectFile, const std::string& assetRoot, std::string& error) {
    std::error_code ec;
    const auto target = fs::absolute(projectFile, ec).lexically_normal();
    if (ec || target.extension() != ".stoneveil") { error = "Choose a .stoneveil project filename."; return false; }
    const auto folder = target.parent_path();
    if (!fs::is_directory(folder, ec) || !fs::is_empty(folder, ec) || ec) {
        error = "Create the project in a new empty folder. Existing files will not be overwritten."; return false;
    }
    ProjectDocument created;
    created.path_ = target.string();
    created.root_ = folder.string();
    created.campaign_.id = "project." + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    created.campaign_.name = target.stem().string();
    created.campaign_.startingLevelId = "level.start";
    created.campaign_.levels.push_back({"level.start", "FIRST LEVEL", "content/levels/start.svl"});
    fs::create_directories(folder / "content/levels", ec);
    if (!ec) fs::create_directories(folder / "content/campaigns", ec);
    if (ec) { error = ec.message(); return false; }
    auto level = LevelDocument::newLevel();
    level.id = "level.start";
    level.name = "FIRST LEVEL";
    if (!LevelIO::save((folder / "content/levels/start.svl").string(), level, error)) return false;
    // Only the engine's bundled starter art/audio are copied, never source/build files.
    for (const auto* category : {"content/textures", "content/audio"}) {
        std::string source;
        if (!resolveInside(assetRoot, category, source) || !fs::exists(source, ec)) continue;
        for (fs::recursive_directory_iterator it(source, ec), end; it != end && !ec; it.increment(ec)) {
            if (it->is_symlink(ec)) { error = "Starter assets may not contain symbolic links."; return false; }
            if (!it->is_regular_file(ec)) continue;
            const auto relative = fs::relative(it->path(), assetRoot, ec);
            if (ec || !copyFile(it->path(), folder / relative, error)) return false;
        }
        if (ec) { error = ec.message(); return false; }
    }
    if (!created.save(error)) return false;
    *this = std::move(created);
    return true;
}

bool ProjectDocument::save(std::string& error) const {
    if (!isOpen()) { error = "Open or create a project first."; return false; }
    std::string registry;
    if (!resolveInside(root_, campaignPath_, registry)) { error = "Unsafe campaign path."; return false; }
    if (!CampaignIO::save(registry, campaign_, error)) return false;
    return writeFileAtomically(path_, manifest(campaign_, campaignPath_), error);
}

std::string ProjectDocument::levelPath(const std::string& id) const {
    for (const auto& entry : campaign_.levels) {
        if (entry.id == id) {
            std::string result;
            if (resolveInside(root_, entry.path, result)) return result;
        }
    }
    return {};
}

bool ProjectDocument::registerLevel(const std::string& levelFile, std::string& error) {
    if (!isOpen()) { error = "Open a project before registering levels."; return false; }
    std::error_code ec;
    const auto relative = fs::relative(levelFile, root_, ec).generic_string();
    std::string resolved;
    if (ec || relative.rfind("content/levels/", 0) != 0 || !resolveInside(root_, relative, resolved)) {
        error = "Save this level inside the project's content/levels folder first."; return false;
    }
    LevelDefinition level;
    if (!LevelIO::load(resolved, level, error)) return false;
    auto updated = campaign_;
    for (const auto& entry : updated.levels) {
        if (entry.id == level.id && entry.path != relative) {
            error = "Another level already uses this ID. Use Save As to create a distinct level."; return false;
        }
    }
    auto found = std::find_if(updated.levels.begin(), updated.levels.end(), [&](const auto& entry) { return entry.path == relative; });
    if (found == updated.levels.end()) updated.levels.push_back({level.id, level.name, relative});
    else {
        if (updated.startingLevelId == found->id) updated.startingLevelId = level.id;
        *found = {level.id, level.name, relative};
    }
    if (updated.levels.size() > 256) { error = "A project supports up to 256 levels."; return false; }
    const auto previous = campaign_;
    campaign_ = std::move(updated);
    if (!save(error)) { campaign_ = previous; return false; }
    return true;
}

bool ProjectDocument::setStartingLevel(const std::string& id, std::string& error) {
    if (levelPath(id).empty()) { error = "Choose a registered starting level."; return false; }
    const auto previous = campaign_.startingLevelId;
    campaign_.startingLevelId = id;
    if (!save(error)) { campaign_.startingLevelId = previous; return false; }
    return true;
}

std::vector<ProjectIssue> ProjectDocument::validate() const {
    std::vector<ProjectIssue> issues;
    for (const auto& error : CampaignIO::validate(campaign_)) issues.push_back({IssueSeverity::Error, error});
    std::set<std::string> paths;
    for (const auto& entry : campaign_.levels) {
        if (!paths.insert(entry.path).second) issues.push_back({IssueSeverity::Error, "Two registry entries point to the same level file."});
        LevelDefinition level;
        std::string error;
        const auto file = levelPath(entry.id);
        if (file.empty() || !LevelIO::load(file, level, error)) {
            issues.push_back({IssueSeverity::Error, entry.name + ": " + (file.empty() ? "Level path escapes the project." : error)});
            continue;
        }
        if (level.id != entry.id) issues.push_back({IssueSeverity::Error, entry.name + ": registry ID does not match the level. Save/register it again."});
        std::set<std::string> textures;
        const auto inspectMaterial = [&](const std::string& id) {
            const auto* material = findMaterial(id);
            if (material && !material->texturePath.empty()) textures.insert(material->texturePath);
        };
        inspectMaterial(level.surfaces.wallMaterial);
        inspectMaterial(level.surfaces.floorMaterial);
        if (level.surfaces.ceilingMode != CeilingMode::Sky) inspectMaterial(level.surfaces.ceilingMaterial);
        for (const auto& surface : level.surfaceOverrides) inspectMaterial(surface.materialId);
        for (const auto& texture : textures) {
            std::string path;
            std::error_code ec;
            if (!resolveInside(root_, texture, path))
                issues.push_back({IssueSeverity::Error, entry.name + ": texture path escapes the project."});
            else if (!fs::is_regular_file(path, ec))
                issues.push_back({IssueSeverity::Warning, entry.name + ": missing texture " + texture + "; a fallback color will be used."});
        }
        if (!level.musicPath.empty()) {
            std::string music;
            std::error_code ec;
            if (!resolveInside(root_, level.musicPath, music)) issues.push_back({IssueSeverity::Error, entry.name + ": music path escapes the project."});
            else if (!fs::is_regular_file(music, ec)) issues.push_back({IssueSeverity::Warning, entry.name + ": music is missing; this level will be silent."});
        }
    }
    return issues;
}

bool ProjectDocument::exportWindowsGame(const std::string& executable, const std::string& destination, std::string& error) const {
    for (const auto& issue : validate()) if (issue.severity == IssueSeverity::Error) { error = issue.message; return false; }
    std::error_code ec;
    const auto target = fs::absolute(destination, ec).lexically_normal();
    if (ec || !fs::is_directory(target, ec) || !fs::is_empty(target, ec) || ec) {
        error = "Export into an existing empty folder. No existing game files will be overwritten."; return false;
    }
    std::set<std::string> assets;
    const auto addMaterial = [&](const std::string& id) {
        const auto* material = findMaterial(id);
        if (material && !material->texturePath.empty()) assets.insert(material->texturePath);
    };
    for (const auto& entry : campaign_.levels) {
        LevelDefinition level;
        if (!LevelIO::load(levelPath(entry.id), level, error)) return false;
        if (!copyFile(levelPath(entry.id), target / entry.path, error)) return false;
        addMaterial(level.surfaces.wallMaterial);
        addMaterial(level.surfaces.floorMaterial);
        if (level.surfaces.ceilingMode != CeilingMode::Sky) addMaterial(level.surfaces.ceilingMaterial);
        for (const auto& surface : level.surfaceOverrides) addMaterial(surface.materialId);
        if (!level.musicPath.empty()) assets.insert(level.musicPath);
        assets.insert("content/textures/doors/iron-banded-wooden-door.png");
        assets.insert("content/textures/doors/secret-stone-door.png");
        assets.insert("content/textures/doors/rusted-iron-gate.png");
    }
    for (const auto& asset : assets) {
        std::string source;
        if (!resolveInside(root_, asset, source)) { error = "An asset path escapes the project: " + asset; return false; }
        if (fs::is_regular_file(source, ec) && !copyFile(source, target / asset, error)) return false;
    }
    std::string registry;
    if (!resolveInside(root_, campaignPath_, registry) || !copyFile(registry, target / campaignPath_, error) ||
        !copyFile(path_, target / "game.stoneveil", error) || !copyFile(executable, target / "stoneveil.exe", error)) return false;
    // Written last: an interrupted export never advertises itself as playable.
    return writeFileAtomically((target / "stoneveil.game").string(), "game.stoneveil\n", error, false);
}
}
