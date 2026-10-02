// Sound effects through SDL3_mixer. Every call is a safe no-op when audio
// or the sound files are missing.
#pragma once

#include <string>

#include <SDL3_mixer/SDL_mixer.h>

namespace flappy {

class Audio {
public:
    bool init(const std::string& asset_dir);
    void play_flap() { play(flap_); }
    void play_score() { play(score_); }
    void play_hit() { play(hit_); }
    void shutdown();
    bool loaded_all() const {
        return flap_ != nullptr && score_ != nullptr && hit_ != nullptr;
    }

private:
    void play(MIX_Audio* audio);

    bool mixer_ready_ = false;
    MIX_Mixer* mixer_ = nullptr;
    MIX_Audio* flap_ = nullptr;
    MIX_Audio* score_ = nullptr;
    MIX_Audio* hit_ = nullptr;
};

}  // namespace flappy
