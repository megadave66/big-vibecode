#pragma once
// Audio via SDL_mixer 3 (MIX_* API). Music loops on one track; SFX are predecoded and fire-and-forget.
// Every call is a safe no-op when audio is muted or no device could be opened (for example
// SDL_AUDIO_DRIVER=dummy still works; a machine with no audio device just stays silent).

#include <SDL3_mixer/SDL_mixer.h>

#include <map>
#include <string>

namespace gd {

enum class Sfx { Death, LevelComplete, Checkpoint, Portal, MenuMove, MenuSelect, Count };

class Audio {
public:
    Audio() = default;
    ~Audio();
    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;

    // dataDir holds assets/. SDL_INIT_AUDIO must already be on (App does it unless --mute).
    // Returns true when audio is running. A false return is not fatal.
    bool init(const std::string& dataDir, bool mute);
    void shutdown();
    bool enabled() const { return mixer_ != nullptr; }

    void playSfx(Sfx s);
    // Start (or restart from the beginning) a looping music file given as a full path.
    void playMusic(const std::string& path);
    void stopMusic(int fadeMs = 0);
    void pauseMusic();
    void resumeMusic();
    bool musicPlaying() const;
    const std::string& currentMusic() const { return musicPath_; }

private:
    MIX_Audio* loadMusic(const std::string& path);

    MIX_Mixer* mixer_ = nullptr;
    MIX_Track* musicTrack_ = nullptr;
    bool mixInit_ = false;
    std::string dataDir_;
    std::string musicPath_;
    MIX_Audio* sfx_[static_cast<int>(Sfx::Count)] = {};
    std::map<std::string, MIX_Audio*> music_;
};

}  // namespace gd
