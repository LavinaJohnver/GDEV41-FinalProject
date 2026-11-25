#ifndef MAIN_MENU_SCENE
#define MAIN_MENU_SCENE

#include <raylib.h>

#include "scene_manager.hpp"
#include "settings_menu.hpp"
#include "ui.hpp"

class MainMenuScene : public Scene {
    const char* title = "<insert title here>";

    MenuButton start_button;
    MenuButton settings_button;
    SettingsMenu settings_menu;

public:
    void Begin() override {
        const float button_width = 220;
        const float button_height = 60;
        const float center_x = (GetScreenWidth() - button_width) / 2;

        start_button    = { { center_x, 260, button_width, button_height }, "Start" };
        settings_button = { { center_x, 350, button_width, button_height }, "Settings" };

        settings_menu.Close();
    }

    void End() override {}

    void Update() override {
        // while settings are open, the menu buttons underneath don't react
        if (settings_menu.IsOpen()) {
            settings_menu.Update();
            return;
        }

        if (start_button.IsClicked() && GetSceneManager() != nullptr) {
            GetSceneManager()->SwitchScene(SCENE_GAME);
            return;
        }

        if (settings_button.IsClicked()) {
            settings_menu.Open(false);
        }
    }

    void Draw() override {
        const int title_size = 50;
        int title_width = MeasureText(title, title_size);
        DrawText(title, (GetScreenWidth() - title_width) / 2, 120, title_size, BLACK);

        start_button.Draw();
        settings_button.Draw();

        settings_menu.Draw();
    }
};

#endif
