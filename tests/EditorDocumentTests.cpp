#include "LevelDocument.hpp"
#include "LevelIO.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

#define CHECK(expression) do { if (!(expression)) { \
    std::cerr << "Failed at line " << __LINE__ << ": " << #expression << '\n'; \
    std::exit(1); } } while (false)

std::string bytes(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

int main() {
    using namespace sv;
    const auto directory = std::filesystem::current_path() /
        ("document-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    CHECK(std::filesystem::create_directory(directory));
    const auto original = (directory / "original.svl").string();
    std::string error;
    CHECK(LevelIO::save(original, levelOneDefinition(), error));
    const auto originalBytes = bytes(original);

    LevelDocument document;
    CHECK(document.open(original, error));
    CHECK(!document.dirty());
    document.beginEdit();
    document.draft().name = "An edited name";
    CHECK(document.dirty());
    CHECK(document.undo());
    CHECK(!document.dirty() && document.draft().name == levelOneDefinition().name);
    CHECK(document.redo() && document.dirty());
    CHECK(document.save(error) && !document.dirty());
    CHECK(document.undo() && document.dirty()); // old content differs from disk
    CHECK(document.redo() && !document.dirty());
    const auto savedOriginal = bytes(original);

    document.replaceUntitled(LevelDocument::newLevel());
    CHECK(document.path().empty() && document.dirty());
    CHECK(!document.save(error)); // untitled never falls through to old file
    CHECK(bytes(original) == savedOriginal);
    CHECK(document.undo() && document.path() == original && !document.dirty());
    CHECK(document.redo() && document.path().empty() && document.dirty());
    CHECK(document.saveCopy(directory.string(), error));
    const auto newPath = document.path();
    const auto newId = document.draft().id;
    CHECK(newPath != original && newId != "level.new" && !document.dirty());
    CHECK(bytes(original) == savedOriginal);

    CHECK(document.undo() && document.path().empty() && document.dirty());
    CHECK(document.redo() && document.path() == newPath && !document.dirty());
    CHECK(document.saveCopy(directory.string(), error));
    const auto copyPath = document.path();
    CHECK(copyPath != newPath && document.draft().id != newId);
    CHECK(document.undo() && document.path() == newPath && !document.dirty());
    CHECK(document.redo() && document.path() == copyPath && !document.dirty());

    const auto beforeFailure = bytes(copyPath);
    CHECK(!document.open((directory / "missing.svl").string(), error));
    CHECK(document.path() == copyPath && !document.dirty());
    document.beginEdit();
    document.draft().map[0] = "#";
    CHECK(!document.save(error) && document.dirty());
    CHECK(bytes(copyPath) == beforeFailure); // validation failure leaves disk intact
    CHECK(!document.saveCopy(directory.string(), error));
    CHECK(document.path() == copyPath && document.dirty());
    CHECK(document.undo() && !document.dirty());

    for (int i = 0; i < 80; ++i) {
        document.beginEdit();
        document.draft().name = std::to_string(i);
    }
    int undos = 0;
    while (document.undo()) ++undos;
    CHECK(undos == 64);
    document.beginEdit();
    document.draft().name = "New branch";
    CHECK(!document.redo());

    CHECK(!originalBytes.empty());
    CHECK(std::filesystem::remove(original));
    CHECK(std::filesystem::remove(newPath));
    CHECK(std::filesystem::remove(copyPath));
    CHECK(std::filesystem::remove(directory));
    return 0;
}
