#ifndef SETTINGS_MENU_HPP
#define SETTINGS_MENU_HPP

#include <raylib.h>

#include "audio_settings.hpp"
#include "ui.hpp"

// What the player picked in the settings menu this frame
enum SettingsAction {
    SETTINGS_NONE,
    SETTINGS_CLOSE,         // Back / Resume, or ESC
    SETTINGS_RESTART,       // in-game only
    SETTINGS_MAIN_MENU,     // in-game only
};

// Settings overlay drawn on top of whatever scene is open.
//
// From the main menu it only shows the volume sliders and Back.
// In game it also shows Restart and Return to Menu.
class SettingsMenu {
    bool open = false;
    bool in_game = false;

    Slider music_slider;
    Slider sfx_slider;
    MenuButton close_button;
    MenuButton restart_button;
    MenuButton menu_button;

    Rectangle panel;

    void Layout() {
        const float sw = (float)GetScreenWidth();
        const float sh = (float)GetScreenHeight();

        const float panel_w = 460;
        const float panel_h = in_game ? 560 : 380;
        panel = { (sw - panel_w) / 2, (sh - panel_h) / 2, panel_w, panel_h };

        const float bar_w = 360;
        const float bar_x = (sw - bar_w) / 2;
        float y = panel.y + 120;

        music_slider = { { bar_x, y, bar_w, 14 }, "Music" };
        y += 90;
        sfx_slider   = { { bar_x, y, bar_w, 14 }, "Sound Effects" };
        y += 60;

        const float button_w = 300;
        const float button_h = 55;
        const float button_x = (sw - button_w) / 2;
        const float gap = 70;

        close_button = { { button_x, y, button_w, button_h }, in_game ? "Resume" : "Back" };
        if (in_game) {
            restart_button = { { button_x, y + gap,     button_w, button_h }, "Restart" };
            menu_button    = { { button_x, y + gap * 2, button_w, button_h }, "Return to Menu" };
        }
    }

public:
    // in_game: true shows Restart / Return to Menu
    void Open(bool in_game) {
        this->in_game = in_game;
        open = true;
        Layout();
    }

    void Close() { open = false; }

    bool IsOpen() const { return open; }

    SettingsAction Update() {
        if (!open) return SETTINGS_NONE;

        AudioSettings* audio = AudioSettings::GetInstance();
        music_slider.Update(audio->MusicVolume());
        sfx_slider.Update(audio->SfxVolume());

        SettingsAction action = SETTINGS_NONE;

        if (IsKeyPressed(KEY_ESCAPE) || close_button.IsClicked()) {
            action = SETTINGS_CLOSE;
        } else if (in_game && restart_button.IsClicked()) {
            action = SETTINGS_RESTART;
        } else if (in_game && menu_button.IsClicked()) {
            action = SETTINGS_MAIN_MENU;
        }

        if (action != SETTINGS_NONE) open = false;
        return action;
    }

    void Draw() const {
        if (!open) return;

        // dim whatever is behind the menu
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.5f));

        DrawRectangleRec(panel, RAYWHITE);
        DrawRectangleLinesEx(panel, 4, DARKGRAY);

        const char* title = in_game ? "Paused" : "Settings";
        const int title_size = 40;
        DrawText(title,
                 (int)(panel.x + (panel.width - MeasureText(title, title_size)) / 2),
                 (int)panel.y + 25, title_size, BLACK);

        AudioSettings* audio = AudioSettings::GetInstance();
        music_slider.Draw(audio->GetMusicVolume());
        sfx_slider.Draw(audio->GetSfxVolume());

        close_button.Draw();
        if (in_game) {
            restart_button.Draw();
            menu_button.Draw();
        }
    }
};

#endif
