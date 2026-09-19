#pragma once

#include "Dungeon.hpp"
#include "EventSystem.hpp"
#include "StoryState.hpp"

namespace sv {

// Compiles author-facing data into one runtime. No window, audio, or hardcoded
// level coordinates live here. Presentation receives ordered notifications.
enum class WorldEventCue { DoorOpened, DoorLocked };
struct WorldEventPresentation {
    std::function<void(const std::string&)> message;
    std::function<void(WorldEventCue)> cue;
    std::function<bool(CharacterId)> recruit;
    std::function<void()> openPartyManagement;
    std::function<bool(const std::string&, const std::string&)> transitionLevel;
};

bool configureWorldEvents(const Dungeon& dungeon, EventRuntime& runtime);
EventFireResult dispatchWorldEvent(EventRuntime& runtime, const EventContext& context,
                                   Dungeon& dungeon, int& campaignKeys,
                                   const WorldEventPresentation& presentation,
                                   StoryState* storyState = nullptr);

} // namespace sv
