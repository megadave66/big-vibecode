#include "Audio.hpp"

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

namespace bac::app {
namespace {

const char* const kFiles[] = {"card_slide.ogg", "card_flip.ogg", "chip_place.ogg",
                              "chip_remove.ogg", "win.ogg",       "lose.ogg"};

}  // namespace

SfxBank::SfxBank(MIX_Mixer* mixer, const std::string& assetDir) : mixer_(mixer) {
    audio_.fill(nullptr);
    warned_.fill(false);
    if (!mixer_) {
        SDL_Log("warning: no audio mixer, sound effects disabled");
        return;
    }
    for (int i = 0; i < kCount; ++i) {
        const std::string path = assetDir + "/sfx/" + kFiles[i];
        audio_[i] = MIX_LoadAudio(mixer_, path.c_str(), true);
        if (!audio_[i]) {
            SDL_Log("warning: could not load %s: %s", path.c_str(), SDL_GetError());
            warned_[i] = true;
        }
    }
}

SfxBank::~SfxBank() {
    for (MIX_Audio* a : audio_)
        if (a) MIX_DestroyAudio(a);
}

bool SfxBank::loaded(Sfx sfx) const { return audio_[static_cast<int>(sfx)] != nullptr; }

void SfxBank::play(Sfx sfx) {
    const int i = static_cast<int>(sfx);
    if (muted_ || !mixer_ || !audio_[i]) return;
    if (!MIX_PlayAudio(mixer_, audio_[i]) && !warned_[i]) {
        warned_[i] = true;
        SDL_Log("warning: could not play %s: %s", kFiles[i], SDL_GetError());
    }
}

}  // namespace bac::app
