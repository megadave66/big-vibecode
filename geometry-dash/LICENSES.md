# Licenses

All assets are CC0 (public domain), except the font, which is SIL OFL-1.1. No CC-BY tracks are used. Licences verified on the source pages.

## Assets

| file(s) | title | author | source URL | licence | notes |
|---|---|---|---|---|---|
| assets/music/track01.ogg | Menu Title 1 (relaxed) (from 12 Music Loops) | SubspaceAudio (Juhani Junkala) | https://opengameart.org/content/12-music-loops | CC0 | trimmed to 52 s, fade-out, loudnorm I=-16, re-encoded OGG |
| assets/music/track02.ogg | Chiptune Adventures: Stage 1 (from 4 Chiptunes (Adventure)) | SubspaceAudio (Juhani Junkala) | https://opengameart.org/content/4-chiptunes-adventure | CC0 | trimmed to 41 s, fade-out, loudnorm I=-16, re-encoded OGG |
| assets/music/track03.ogg | Chiptune Adventures: Stage 2 (from 4 Chiptunes (Adventure)) | SubspaceAudio (Juhani Junkala) | https://opengameart.org/content/4-chiptunes-adventure | CC0 | trimmed to 52 s, fade-out, loudnorm I=-16, re-encoded OGG |
| assets/music/track04.ogg | Retro Sports: Stage 3 (from 12 Music Loops) | SubspaceAudio (Juhani Junkala) | https://opengameart.org/content/12-music-loops | CC0 | trimmed to 52 s, fade-out, loudnorm I=-16, re-encoded OGG |
| assets/music/track05.ogg | Retro Game Music Pack: Level 1 (from 5 Chiptunes (Action)) | SubspaceAudio (Juhani Junkala) | https://opengameart.org/content/5-chiptunes-action | CC0 | trimmed to 52 s, fade-out, loudnorm I=-16, re-encoded OGG |
| assets/music/track06.ogg | Super Action Chiptunes: Stage 6 (from 12 Music Loops) | SubspaceAudio (Juhani Junkala) | https://opengameart.org/content/12-music-loops | CC0 | trimmed to 52 s, fade-out, loudnorm I=-16, re-encoded OGG |
| assets/music/track07.ogg | Retro Game Music Pack: Level 2 (from 5 Chiptunes (Action)) | SubspaceAudio (Juhani Junkala) | https://opengameart.org/content/5-chiptunes-action | CC0 | trimmed to 52 s, fade-out, loudnorm I=-16, re-encoded OGG |
| assets/music/track08.ogg | Super Action Chiptunes: Stage 7 (from 12 Music Loops) | SubspaceAudio (Juhani Junkala) | https://opengameart.org/content/12-music-loops | CC0 | trimmed to 52 s, fade-out, loudnorm I=-16, re-encoded OGG |
| assets/music/track09.ogg | Retro Game Music Pack: Level 3 (from 5 Chiptunes (Action)) | SubspaceAudio (Juhani Junkala) | https://opengameart.org/content/5-chiptunes-action | CC0 | trimmed to 52 s, fade-out, loudnorm I=-16, re-encoded OGG |
| assets/music/track10.ogg | Heavy Metal Chiptunes: Stage F (faster) (from 12 Music Loops) | SubspaceAudio (Juhani Junkala) | https://opengameart.org/content/12-music-loops | CC0 | trimmed to 52 s, fade-out, loudnorm I=-16, re-encoded OGG |
| assets/sfx/death.ogg | Digital Audio: lowRandom | Kenney | https://kenney.nl/assets/digital-audio | CC0 | unmodified copy |
| assets/sfx/level_complete.ogg | Digital Audio: powerUp12 | Kenney | https://kenney.nl/assets/digital-audio | CC0 | unmodified copy |
| assets/sfx/checkpoint.ogg | Digital Audio: pepSound3 | Kenney | https://kenney.nl/assets/digital-audio | CC0 | unmodified copy |
| assets/sfx/portal.ogg | Digital Audio: phaserUp3 | Kenney | https://kenney.nl/assets/digital-audio | CC0 | unmodified copy |
| assets/sfx/menu_move.ogg | Interface Sounds: switch_001 | Kenney | https://kenney.nl/assets/interface-sounds | CC0 | unmodified copy |
| assets/sfx/menu_select.ogg | Interface Sounds: confirmation_001 | Kenney | https://kenney.nl/assets/interface-sounds | CC0 | unmodified copy |
| assets/fonts/RussoOne-Regular.ttf, assets/fonts/OFL-RussoOne.txt | Russo One | Jovanny Lemonad | https://github.com/google/fonts/tree/main/ofl/russoone | OFL-1.1 | unmodified copy; licence text included; replaces Kenney Future (K/X/H looked alike) |

## Libraries

Fetched at configure time by CMake FetchContent (pinned release tarballs with SHA256). Not redistributed in this folder.

| library | version | source | licence |
|---|---|---|---|
| SDL3 | 3.4.16 | https://github.com/libsdl-org/SDL | zlib |
| SDL3_ttf | 3.2.2 | https://github.com/libsdl-org/SDL_ttf | zlib |
| FreeType (SDL fork, vendored by SDL_ttf) | VER-2-13-2-SDL @ 9973564c | https://github.com/libsdl-org/freetype | FreeType License (FTL), dual with GPLv2; used under FTL |
| SDL3_mixer | 3.2.4 | https://github.com/libsdl-org/SDL_mixer | zlib |
| stb_vorbis (bundled in SDL_mixer) | as bundled | https://github.com/nothings/stb | MIT or public domain |
| nlohmann/json | 3.12.0 | https://github.com/nlohmann/json | MIT |
| doctest (tests only) | 2.5.3 | https://github.com/doctest/doctest | MIT |

## Other files
- `assets/music/tracks.json`: track metadata written for this project (no third-party content).
- `tools/gen_music.py`: original chiptune generator written for this project. Its output is not shipped; all 10 tracks are the CC0 recordings above.
- All game art (player, blocks, spikes, portals, backgrounds, particles) is drawn procedurally in code (`src/app/GameRenderer.cpp`). No sprite files.
