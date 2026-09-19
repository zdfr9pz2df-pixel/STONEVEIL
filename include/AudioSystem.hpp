#pragma once

#include "raylib.h"

#include <string>
#include <unordered_map>

namespace sv {

enum class AudioCue {
    UiConfirm,
    UiBack,
    Step,
    Turn,
    Bump,
    DoorOpen,
    DoorLocked,
    Secret,
    Pickup,
    Heal,
    Attack,
    Hit,
    Kill,
    Save,
    Error,
    Victory,
    Defeat,
};

class AudioSystem {
public:
    AudioSystem() = default;
    ~AudioSystem();

    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;

    void initialize();
    void shutdown();
    void update();
    void play(AudioCue cue);
    void playMusic(const std::string& relativePath);
    void stopMusic();
    void setContentRoot(std::string root) { stopMusic(); contentRoot_ = std::move(root); }
    void setMuted(bool muted);
    void toggleMuted();

    bool ready() const { return initialized_ && IsAudioDeviceReady(); }
    bool muted() const { return muted_; }
    const std::string& currentMusicPath() const { return currentMusicPath_; }

private:
    Sound makeCue(float frequency, float durationSeconds, float volume, float decay) const;
    void registerCue(AudioCue cue, float frequency, float durationSeconds, float volume, float decay);
    std::string resolveMusicPath(const std::string& relativePath) const;
    void unloadMusic();

    std::unordered_map<AudioCue, Sound> sounds_;
    Music music_{};
    std::string currentMusicPath_;
    std::string contentRoot_;
    bool initialized_{false};
    bool muted_{false};
    bool musicLoaded_{false};
};

} // namespace sv
