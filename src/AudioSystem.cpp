#include "AudioSystem.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#ifndef STONEVEIL_SOURCE_DIR
#define STONEVEIL_SOURCE_DIR ""
#endif

namespace sv {
namespace {
constexpr int SampleRate = 22050;
constexpr float Tau = 6.28318530717958647692f;
constexpr float MasterVolume = 0.32f;
constexpr float MusicVolume = 0.28f;

float envelope(float progress, float decay) {
    const float attack = std::min(progress / 0.08f, 1.0f);
    const float release = std::max(0.0f, 1.0f - progress);
    return attack * std::pow(release, decay);
}

bool soundReady(const Sound& sound) {
    return sound.stream.buffer != nullptr;
}

bool musicReady(const Music& music) {
    return music.stream.buffer != nullptr;
}
}

AudioSystem::~AudioSystem() {
    shutdown();
}

void AudioSystem::initialize() {
    if (initialized_) return;

    InitAudioDevice();
    if (!IsAudioDeviceReady()) return;

    SetMasterVolume(MasterVolume);
    registerCue(AudioCue::UiConfirm, 660.0f, 0.075f, 0.20f, 1.7f);
    registerCue(AudioCue::UiBack, 330.0f, 0.085f, 0.18f, 1.5f);
    registerCue(AudioCue::Step, 92.0f, 0.060f, 0.17f, 2.4f);
    registerCue(AudioCue::Turn, 140.0f, 0.050f, 0.13f, 2.0f);
    registerCue(AudioCue::Bump, 74.0f, 0.115f, 0.25f, 1.2f);
    registerCue(AudioCue::DoorOpen, 112.0f, 0.180f, 0.23f, 1.1f);
    registerCue(AudioCue::DoorLocked, 186.0f, 0.105f, 0.21f, 1.8f);
    registerCue(AudioCue::Secret, 244.0f, 0.220f, 0.18f, 0.9f);
    registerCue(AudioCue::Pickup, 880.0f, 0.105f, 0.20f, 1.4f);
    registerCue(AudioCue::Heal, 523.0f, 0.180f, 0.18f, 1.0f);
    registerCue(AudioCue::Attack, 180.0f, 0.070f, 0.20f, 1.2f);
    registerCue(AudioCue::Hit, 118.0f, 0.105f, 0.24f, 1.0f);
    registerCue(AudioCue::Kill, 98.0f, 0.240f, 0.26f, 0.8f);
    registerCue(AudioCue::Save, 740.0f, 0.140f, 0.17f, 1.2f);
    registerCue(AudioCue::Error, 155.0f, 0.120f, 0.20f, 1.0f);
    registerCue(AudioCue::Victory, 784.0f, 0.300f, 0.18f, 0.7f);
    registerCue(AudioCue::Defeat, 82.0f, 0.350f, 0.22f, 0.7f);

    initialized_ = true;
}

void AudioSystem::shutdown() {
    if (!initialized_) return;

    unloadMusic();
    for (auto& [cue, sound] : sounds_) {
        (void)cue;
        if (soundReady(sound)) UnloadSound(sound);
    }
    sounds_.clear();
    if (IsAudioDeviceReady()) CloseAudioDevice();
    initialized_ = false;
}

void AudioSystem::update() {
    if (!ready() || !musicLoaded_ || muted_) return;
    UpdateMusicStream(music_);
}

void AudioSystem::play(AudioCue cue) {
    if (!ready() || muted_) return;
    const auto found = sounds_.find(cue);
    if (found == sounds_.end() || !soundReady(found->second)) return;
    PlaySound(found->second);
}

void AudioSystem::playMusic(const std::string& relativePath) {
    if (relativePath.empty()) {
        stopMusic();
        return;
    }
    if (!ready()) {
        currentMusicPath_ = relativePath;
        return;
    }
    if (musicLoaded_ && currentMusicPath_ == relativePath) {
        if (!muted_) ResumeMusicStream(music_);
        return;
    }

    unloadMusic();
    const std::string resolvedPath = resolveMusicPath(relativePath);
    music_ = LoadMusicStream(resolvedPath.c_str());
    if (!musicReady(music_)) {
        TraceLog(LOG_WARNING, "STONEVEIL: failed to load music '%s'", relativePath.c_str());
        currentMusicPath_.clear();
        return;
    }

    music_.looping = true;
    SetMusicVolume(music_, MusicVolume);
    currentMusicPath_ = relativePath;
    musicLoaded_ = true;
    PlayMusicStream(music_);
    if (muted_) PauseMusicStream(music_);
}

void AudioSystem::stopMusic() {
    unloadMusic();
    currentMusicPath_.clear();
}

void AudioSystem::setMuted(bool muted) {
    if (muted_ == muted) return;
    muted_ = muted;
    if (!musicLoaded_ || !ready()) return;
    if (muted_) PauseMusicStream(music_);
    else ResumeMusicStream(music_);
}

void AudioSystem::toggleMuted() {
    setMuted(!muted_);
}

Sound AudioSystem::makeCue(float frequency, float durationSeconds, float volume, float decay) const {
    const int sampleCount = std::max(1, static_cast<int>(static_cast<float>(SampleRate) * durationSeconds));
    std::vector<std::int16_t> samples(static_cast<std::size_t>(sampleCount));
    for (int index = 0; index < sampleCount; ++index) {
        const float progress = static_cast<float>(index) / static_cast<float>(sampleCount);
        const float wobble = 1.0f + 0.018f * std::sin(Tau * progress * 5.0f);
        const float wave = std::sin(Tau * frequency * wobble * static_cast<float>(index) /
                                    static_cast<float>(SampleRate));
        const float harmonics = wave + 0.28f * std::sin(Tau * frequency * 2.0f *
                                                        static_cast<float>(index) /
                                                        static_cast<float>(SampleRate));
        const float shaped = std::clamp(harmonics * envelope(progress, decay) * volume, -1.0f, 1.0f);
        samples[static_cast<std::size_t>(index)] = static_cast<std::int16_t>(shaped * 32767.0f);
    }

    Wave wave{};
    wave.frameCount = static_cast<unsigned int>(sampleCount);
    wave.sampleRate = SampleRate;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.data = samples.data();
    return LoadSoundFromWave(wave);
}

void AudioSystem::registerCue(AudioCue cue, float frequency, float durationSeconds, float volume, float decay) {
    Sound sound = makeCue(frequency, durationSeconds, volume, decay);
    if (soundReady(sound)) {
        SetSoundVolume(sound, volume);
        sounds_.emplace(cue, sound);
    }
}

std::string AudioSystem::resolveMusicPath(const std::string& relativePath) const {
    if (!contentRoot_.empty()) return (std::filesystem::path{contentRoot_} / relativePath).string();
    std::vector<std::filesystem::path> candidates;
    candidates.emplace_back(std::filesystem::path{GetApplicationDirectory()} / relativePath);
    std::error_code error;
    const auto workingDirectory = std::filesystem::current_path(error);
    if (!error) candidates.emplace_back(workingDirectory / relativePath);
    if (std::string{STONEVEIL_SOURCE_DIR}.size() > 0) {
        candidates.emplace_back(std::filesystem::path{STONEVEIL_SOURCE_DIR} / relativePath);
    }

    for (const auto& path : candidates) {
        if (std::filesystem::exists(path, error)) return path.string();
        error.clear();
    }

    TraceLog(LOG_WARNING, "STONEVEIL: missing music file '%s'", relativePath.c_str());
    for (const auto& path : candidates) {
        const std::string nativePath = path.string();
        TraceLog(LOG_WARNING, "STONEVEIL: searched '%s'", nativePath.c_str());
    }
    return candidates.empty() ? relativePath : candidates.front().string();
}

void AudioSystem::unloadMusic() {
    if (!musicLoaded_) return;
    if (ready() && musicReady(music_)) {
        StopMusicStream(music_);
        UnloadMusicStream(music_);
    }
    music_ = Music{};
    musicLoaded_ = false;
}

} // namespace sv
