#pragma once

#include "EventSystem.hpp"

#include <string>
#include <vector>

namespace sv {

struct SpeechCue {
    std::string speakerId;
    std::string text;
    std::string voiceAssetId;
};

struct StoryTileBehavior {
    std::string id;
    std::string name;
    int x{};
    int y{};
    bool once{true};
    std::vector<SpeechCue> speech;
    std::string soundAssetId;
    std::string musicAssetId;
    std::vector<EventAction> thenActions;
};

struct DoorBehavior {
    std::string id;
    std::string name;
    int x{};
    int y{};

    bool opens{true};
    bool locked{false};
    std::string requiredItemId;
    std::string requiredItemCountFact;
    int requiredItemCount{1};

    SpeechCue lockedSpeech;
    std::string lockedMessage;
    std::string openMessage;
    std::string openSoundAssetId;

    bool boobyTrapped{false};
    int trapDamage{0};
    std::string cutsceneId;

    std::vector<EventAction> afterOpenActions;
};

std::string spatialObjectId(const std::string& kind, int x, int y);
EventDefinition compileStoryTile(const StoryTileBehavior& behavior);
std::vector<EventDefinition> compileDoorBehavior(const DoorBehavior& behavior);

} // namespace sv
