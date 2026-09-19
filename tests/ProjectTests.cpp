#include "Project.hpp"
#include "AtomicFile.hpp"
#include "LevelDocument.hpp"
#include "LevelIO.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

#define CHECK(expression) do { if (!(expression)) { std::cerr << "Failed line " << __LINE__ << ": " << #expression << '\n'; std::exit(1); } } while (false)
namespace fs = std::filesystem;
std::string bytes(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
int main(int argc, char** argv) {
    using namespace sv;
    const auto root = fs::current_path() / ("project-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    CHECK(fs::create_directory(root));
    CHECK(fs::create_directory(root / "source"));
    CHECK(fs::create_directory(root / "export"));
    std::string error, resolved;
    ProjectDocument project;
    const auto manifest = (root / "source/story.stoneveil").string();
    CHECK(project.create(manifest, STONEVEIL_SOURCE_DIR, error));
    CHECK(project.isOpen() && project.campaign().levels.size() == 1);
    auto characters = project.characters();
    auto recruit = characters.front();
    recruit.id = 9001; recruit.stableKey = "test.recruit"; recruit.name = "Test Recruit";
    recruit.starter = false; recruit.recruitmentText = "Join us.";
    characters.push_back(recruit);
    CHECK(project.saveCharacters(characters, error));
    CHECK(!project.create(manifest, STONEVEIL_SOURCE_DIR, error));
    CHECK(fs::equivalent(project.path(), manifest));
    CHECK(!ProjectDocument::resolveInside(project.root(), "../outside", resolved));
    CHECK(!ProjectDocument::resolveInside(project.root(), "C:/outside", resolved));
    CHECK(!ProjectDocument::resolveInside(project.root(), "content/../../outside", resolved));
    CHECK(ProjectDocument::resolveInside(project.root(), "content/levels/start.svl", resolved));
    LevelDocument level;
    CHECK(level.open(resolved, error));
    CHECK(level.saveCopyTo((root / "source/content/levels/Mead Hall.svl").string(), error));
    CHECK(level.draft().id.find(' ') == std::string::npos);
    CHECK(project.registerLevel(level.path(), error));
    CHECK(project.campaign().levels.size() == 2);
    CHECK(project.setStartingLevel(level.draft().id, error));
    ProjectDocument reopened;
    CHECK(reopened.open(manifest, error));
    CHECK(reopened.campaign().startingLevelId == level.draft().id);
    CHECK(reopened.characters().size() == 4 && reopened.characters().back().id == 9001);
    CHECK(!reopened.open((root / "missing.stoneveil").string(), error));
    CHECK(fs::equivalent(reopened.path(), manifest));
    CHECK(LevelIO::save((root / "source/content/levels/duplicate.svl").string(), level.draft(), error));
    CHECK(!project.registerLevel((root / "source/content/levels/duplicate.svl").string(), error));
    CHECK(project.campaign().levels.size() == 2);
    level.beginEdit();
    level.draft().objects.push_back({"recruit.test", WorldObjectKind::Recruit, 3, 3,
                                     "Test Recruit", "Join us.", true, 9001});
    CHECK(level.save(error));
    for (const auto& issue : project.validate()) CHECK(issue.severity != IssueSeverity::Error);
    level.beginEdit();
    level.draft().objects.back().characterId = 777777;
    CHECK(level.save(error));
    bool missingRecruitError = false;
    for (const auto& issue : project.validate())
        missingRecruitError = missingRecruitError || (issue.severity == IssueSeverity::Error &&
            issue.message.find("missing character") != std::string::npos);
    CHECK(missingRecruitError);
    level.beginEdit();
    level.draft().objects.back().characterId = 9001;
    level.draft().musicPath = "content/audio/music/missing.ogg";
    CHECK(level.save(error));
    CHECK(!project.saveCharacters(characterDefinitions(), error));
    CHECK(error.find("recruit point") != std::string::npos);
    CHECK(project.characters().size() == 4);
    bool warning = false;
    for (const auto& issue : project.validate()) {
        CHECK(issue.severity != IssueSeverity::Error);
        warning = warning || issue.severity == IssueSeverity::Warning;
    }
    CHECK(warning);
    const auto atomic = (root / "atomic.txt").string();
    CHECK(writeFileAtomically(atomic, "original", error));
    CHECK(!writeFileAtomically(atomic, "replacement", error, false));
    CHECK(bytes(atomic) == "original");
    CHECK(writeFileAtomically(atomic, "replacement", error));
    CHECK(bytes(atomic) == "replacement");
    CHECK(!writeFileAtomically((root / "source").string(), "bad", error));
    CHECK(fs::is_directory(root / "source"));
    const auto executable = argc > 1 ? std::string{argv[1]} : (root / "engine.exe").string();
    if (argc <= 1) CHECK(writeFileAtomically(executable, "test executable", error));
    CHECK(project.exportWindowsGame(executable, (root / "export").string(), error));
    CHECK(bytes(root / "export/stoneveil.game") == "game.stoneveil\n");
    CHECK(bytes(root / "export/stoneveil.exe") == bytes(executable));
    CHECK(!fs::exists(root / "export/content/levels/duplicate.svl"));
    ProjectDocument exported;
    CHECK(exported.open((root / "export/game.stoneveil").string(), error));
    CHECK(exported.campaign().startingLevelId == level.draft().id);
    CHECK(exported.characters().size() == 4 && exported.characters().back().recruitmentText == "Join us.");
    CHECK(!project.exportWindowsGame(executable, (root / "export").string(), error));
    CHECK(bytes(root / "export/stoneveil.exe") == bytes(executable));
    if (argc > 1) { std::cout << "Retained smoke project: " << root.string() << '\n'; return 0; }
    // Only this test-created, unique directory is removed.
    CHECK(root.parent_path() == fs::current_path() && root.filename().string().rfind("project-test-", 0) == 0);
    fs::remove_all(root);
    return 0;
}
