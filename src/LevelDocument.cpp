#include "LevelDocument.hpp"
#include "LevelIO.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>

namespace sv {

bool LevelDocument::dirty() const {
    const auto saved = savedRevisions_.find(current_.path);
    return current_.path.empty() || saved == savedRevisions_.end() || saved->second != current_.revision;
}

bool LevelDocument::open(const std::string& path, std::string& error) {
    LevelDefinition loaded;
    if (!LevelIO::load(path, loaded, error)) return false;
    current_ = {std::move(loaded), path, nextRevision_++};
    savedRevisions_[path] = current_.revision;
    undo_.clear();
    redo_.clear();
    return true;
}

bool LevelDocument::save(std::string& error) {
    if (current_.path.empty()) { error = "Choose a file for this new level with Save As."; return false; }
    if (!LevelIO::save(current_.path, current_.level, error)) return false;
    savedRevisions_[current_.path] = current_.revision;
    return true;
}

bool LevelDocument::saveCopy(const std::string& directory, std::string& error) {
    std::string stem = current_.level.id.empty() ? "new-level" : current_.level.id;
    std::replace_if(stem.begin(), stem.end(), [](unsigned char ch) {
        return !std::isalnum(ch) && ch != '-' && ch != '_';
    }, '-');
    std::error_code ec;
    for (int suffix = 1; suffix <= 10000; ++suffix) {
        const auto filename = stem + (suffix == 1 ? "" : "-" + std::to_string(suffix));
        const auto candidate = std::filesystem::path{directory} / (filename + ".svl");
        const bool exists = std::filesystem::exists(candidate, ec);
        if (ec) { error = "Cannot inspect the destination folder: " + ec.message(); return false; }
        const auto id = "level." + filename;
        if (exists || id == current_.level.id) continue;
        auto copy = current_.level;
        copy.id = id;
        if (!LevelIO::save(candidate.string(), copy, error)) return false;
        beginEdit();
        current_.level = std::move(copy);
        current_.path = candidate.string();
        savedRevisions_[current_.path] = current_.revision;
        return true;
    }
    error = "No unused level filename was available. Choose another level ID.";
    return false;
}

bool LevelDocument::saveCopyTo(const std::string& path, std::string& error) {
    const std::filesystem::path destination{path};
    std::error_code ec;
    if (destination.extension() != ".svl" || std::filesystem::exists(destination, ec) || ec) {
        error = "Choose a new .svl filename. Use Save to update the current file."; return false;
    }
    auto copy = current_.level;
    auto stem = destination.stem().string();
    std::replace_if(stem.begin(), stem.end(), [](unsigned char ch) {
        return !std::isalnum(ch) && ch != '-' && ch != '_';
    }, '-');
    copy.id = "level." + stem + "." + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    if (!LevelIO::save(path, copy, error)) return false;
    beginEdit();
    current_.level = std::move(copy);
    current_.path = path;
    savedRevisions_[path] = current_.revision;
    return true;
}

void LevelDocument::beginEdit() {
    undo_.push_back(current_);
    if (undo_.size() > 64) undo_.erase(undo_.begin());
    redo_.clear();
    current_.revision = nextRevision_++;
}

void LevelDocument::replaceUntitled(LevelDefinition level) {
    beginEdit();
    current_.level = std::move(level);
    current_.path.clear();
}

bool LevelDocument::undo() {
    if (undo_.empty()) return false;
    redo_.push_back(current_);
    current_ = std::move(undo_.back());
    undo_.pop_back();
    return true;
}

bool LevelDocument::redo() {
    if (redo_.empty()) return false;
    undo_.push_back(current_);
    current_ = std::move(redo_.back());
    redo_.pop_back();
    return true;
}

LevelDefinition LevelDocument::newLevel() {
    LevelDefinition fresh;
    fresh.id = "level.new";
    fresh.name = "NEW STORY LEVEL";
    fresh.map.assign(16, std::string(16, '.'));
    std::fill(fresh.map.front().begin(), fresh.map.front().end(), '#');
    std::fill(fresh.map.back().begin(), fresh.map.back().end(), '#');
    for (auto& row : fresh.map) { row.front() = '#'; row.back() = '#'; }
    fresh.map[13][13] = 'E';
    return fresh;
}

} // namespace sv
