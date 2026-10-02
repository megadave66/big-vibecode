#include "Audio.h"

#include <SDL3/SDL.h>

#include <cstdio>

namespace gd {

namespace {
const char* sfxFile(Sfx s) {
    switch (s) {
        case Sfx::Death: return "sfx/death.ogg";
        case Sfx::LevelComplete: return "sfx/level_complete.ogg";
        case Sfx::Checkpoint: return "sfx/checkpoint.ogg";
        case Sfx::Portal: return "sfx/portal.ogg";
        case Sfx::MenuMove: return "sfx/menu_move.ogg";
        case Sfx::MenuSelect: return "sfx/menu_select.ogg";
        case Sfx::Count: break;
    }
    return "";
}
}  // namespace

Audio::~Audio() { shutdown(); }

bool Audio::init(const std::string& dataDir, bool mute) {
    dataDir_ = dataDir;
    if (mute) return false;
    if (!(SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO)) return false;
    if (!MIX_Init()) {
        std::fprintf(stderr, "warning: MIX_Init failed: %s\n", SDL_GetError());
        return false;
    }
    mixInit_ = true;
    mixer_ = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (!mixer_) {
        std::fprintf(stderr, "warning: no audio mixer: %s\n", SDL_GetError());
        return false;
    }
    MIX_SetMixerGain(mixer_, 0.8f);
    musicTrack_ = MIX_CreateTrack(mixer_);
    if (musicTrack_) MIX_SetTrackGain(musicTrack_, 0.55f);
    for (int i = 0; i < static_cast<int>(Sfx::Count); ++i) {
        const std::string p = dataDir_ + "/assets/" + sfxFile(static_cast<Sfx>(i));
        sfx_[i] = MIX_LoadAudio(mixer_, p.c_str(), true);
        if (!sfx_[i]) std::fprintf(stderr, "warning: cannot load %s: %s\n", p.c_str(), SDL_GetError());
    }
    return true;
}

void Audio::shutdown() {
    if (musicTrack_) MIX_DestroyTrack(musicTrack_);
    musicTrack_ = nullptr;
    for (auto& a : sfx_) {
        if (a) MIX_DestroyAudio(a);
        a = nullptr;
    }
    for (auto& kv : music_)
        if (kv.second) MIX_DestroyAudio(kv.second);
    music_.clear();
    if (mixer_) MIX_DestroyMixer(mixer_);
    mixer_ = nullptr;
    if (mixInit_) MIX_Quit();
    mixInit_ = false;
}

void Audio::playSfx(Sfx s) {
    if (!mixer_) return;
    MIX_Audio* a = sfx_[static_cast<int>(s)];
    if (a) MIX_PlayAudio(mixer_, a);
}

MIX_Audio* Audio::loadMusic(const std::string& path) {
    auto it = music_.find(path);
    if (it != music_.end()) return it->second;
    MIX_Audio* a = MIX_LoadAudio(mixer_, path.c_str(), false);  // streamed, not predecoded
    if (!a) std::fprintf(stderr, "warning: cannot load music %s: %s\n", path.c_str(), SDL_GetError());
    music_[path] = a;
    return a;
}

void Audio::playMusic(const std::string& path) {
    if (!mixer_ || !musicTrack_) return;
    MIX_Audio* a = loadMusic(path);
    if (!a) return;
    MIX_StopTrack(musicTrack_, 0);
    MIX_SetTrackAudio(musicTrack_, a);
    MIX_SetTrackLoops(musicTrack_, -1);  // loop forever
    if (!MIX_PlayTrack(musicTrack_, 0)) std::fprintf(stderr, "warning: MIX_PlayTrack: %s\n", SDL_GetError());
    musicPath_ = path;
}

void Audio::stopMusic(int fadeMs) {
    if (!mixer_ || !musicTrack_) return;
    MIX_StopTrack(musicTrack_, fadeMs > 0 ? MIX_TrackMSToFrames(musicTrack_, fadeMs) : 0);
}

void Audio::pauseMusic() {
    if (mixer_ && musicTrack_) MIX_PauseTrack(musicTrack_);
}

void Audio::resumeMusic() {
    if (mixer_ && musicTrack_) MIX_ResumeTrack(musicTrack_);
}

bool Audio::musicPlaying() const { return mixer_ && musicTrack_ && MIX_TrackPlaying(musicTrack_); }

}  // namespace gd
