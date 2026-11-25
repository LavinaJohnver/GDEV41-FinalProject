#ifndef AUDIO_SETTINGS_HPP
#define AUDIO_SETTINGS_HPP

#include <raylib.h>

// Holds the player's music and SFX volumes (0 = silent, 1 = full).
// Implemented as a singleton so every scene sees the same values.
//
// To make new sounds respect the settings, go through the helpers:
//   AudioSettings::GetInstance()->PlaySfx(jump_sound);
//   AudioSettings::GetInstance()->UpdateMusic(bg_music);   // once per frame
class AudioSettings {
    float music_volume = 0.7f;
    float sfx_volume = 0.7f;

    AudioSettings() {}

public:
    AudioSettings(const AudioSettings&) = delete;
    void operator=(const AudioSettings&) = delete;

    static AudioSettings* GetInstance() {
        static AudioSettings instance;
        return &instance;
    }

    float* MusicVolume() { return &music_volume; }
    float* SfxVolume()   { return &sfx_volume; }

    float GetMusicVolume() const { return music_volume; }
    float GetSfxVolume() const   { return sfx_volume; }

    // Plays a sound effect at the current SFX volume
    void PlaySfx(Sound sound) {
        SetSoundVolume(sound, sfx_volume);
        PlaySound(sound);
    }

    // Keeps a music stream playing at the current music volume
    void UpdateMusic(Music music) {
        SetMusicVolume(music, music_volume);
        UpdateMusicStream(music);
    }
};

#endif
