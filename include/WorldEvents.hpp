#pragma once

#include "Dungeon.hpp"
#include "EventSystem.hpp"

namespace sv {

// Compiles author-facing data into one runtime. No window, audio, or hardcoded
// level coordinates live here. Presentation receives ordered notifications.
enum class WorldEventCue { DoorOpened, DoorLocked };
struct WorldEventPresentation {
    std::function<void(const std::string&)> message;
    std::function<void(WorldEventCue)> cue;
    std::function<bool(CharacterId)> recruit;
    std::function<void()> openPartyManagement;
};

bool configureWorldEvents(const Dungeon& dungeon, EventRuntime& runtime);
EventFireResult dispatchWorldEvent(EventRuntime& runtime, const EventContext& context,
                                   Dungeon& dungeon, int& campaignKeys,
                                   const WorldEventPresentation& presentation);

} // namespace sv
