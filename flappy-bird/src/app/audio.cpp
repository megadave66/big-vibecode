// Section 5 — Audio: load flap/score/hit from asset_dir/audio/, try .ogg then .wav.
#include "audio.hpp"

#include <SDL3/SDL_log.h>

namespace flappy {

namespace {
constexpr float MIXER_GAIN = 0.6f;

// Try to load audio from asset_dir/audio/name.ogg, then name.wav.
// Returns nullptr and logs once if both fail; ignores failures.
MIX_Audio* load_audio(MIX_Mixer* mixer, const std::string& asset_dir,
                      const char* name) {
    if (mixer == nullptr) return nullptr;

    std::string ogg_path = asset_dir + "audio/" + name + ".ogg";
    MIX_Audio* audio = MIX_LoadAudio(mixer, ogg_path.c_str(), true);
    if (audio != nullptr) return audio;

    std::string wav_path = asset_dir + "audio/" + name + ".wav";
    audio = MIX_LoadAudio(mixer, wav_path.c_str(), true);
    if (audio != nullptr) return audio;

    SDL_Log("audio: could not load %s: neither .ogg nor .wav found", name);
    return nullptr;
}
}  // namespace

bool Audio::init(const std::string& asset_dir) {
    if (!MIX_Init()) {
        return false;
    }
    mixer_ready_ = true;
    mixer_ = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (mixer_ == nullptr) {
        return false;
    }

    // Set mixer volume to a sensible level.
    MIX_SetMixerGain(mixer_, MIXER_GAIN);

    // Load audio files; failures are logged but do not fail init.
    flap_ = load_audio(mixer_, asset_dir, "flap");
    score_ = load_audio(mixer_, asset_dir, "score");
    hit_ = load_audio(mixer_, asset_dir, "hit");

    return true;
}

void Audio::play(MIX_Audio* audio) {
    if (mixer_ != nullptr && audio != nullptr) {
        MIX_PlayAudio(mixer_, audio);
    }
}

void Audio::shutdown() {
    for (MIX_Audio** a : {&flap_, &score_, &hit_}) {
        if (*a != nullptr) {
            MIX_DestroyAudio(*a);
            *a = nullptr;
        }
    }
    if (mixer_ != nullptr) {
        MIX_DestroyMixer(mixer_);
        mixer_ = nullptr;
    }
    if (mixer_ready_) {
        MIX_Quit();
        mixer_ready_ = false;
    }
}

}  // namespace flappy
