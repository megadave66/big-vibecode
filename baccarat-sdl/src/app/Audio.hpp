#pragma once
// Sound effects bank on SDL_mixer 3. Fire-and-forget playback.
// A null mixer or a missing file makes play() a silent no-op.

#include <array>
#include <string>

struct MIX_Mixer;
struct MIX_Audio;

namespace bac::app {

enum class Sfx { CardSlide, CardFlip, ChipPlace, ChipRemove, Win, Lose };

class SfxBank {
public:
    // mixer may be null. Files: assetDir + "/sfx/<name>.ogg".
    SfxBank(MIX_Mixer* mixer, const std::string& assetDir);
    ~SfxBank();
    SfxBank(const SfxBank&) = delete;
    SfxBank& operator=(const SfxBank&) = delete;

    void play(Sfx sfx);
    void setMuted(bool muted) { muted_ = muted; }
    bool muted() const { return muted_; }
    bool loaded(Sfx sfx) const;

private:
    static constexpr int kCount = 6;
    MIX_Mixer* mixer_ = nullptr;
    std::array<MIX_Audio*, kCount> audio_{};
    std::array<bool, kCount> warned_{};
    bool muted_ = false;
};

}  // namespace bac::app
