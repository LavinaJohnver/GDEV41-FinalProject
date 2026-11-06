#ifndef MAIN_MENU_SCENE
#define MAIN_MENU_SCENE

#include <raylib.h>

#include "scene_manager.hpp"

// Simple clickable button
struct MenuButton {
    Rectangle rect;
    const char* label;

    bool IsHovered() const {
        return CheckCollisionPointRec(GetMousePosition(), rect);
    }

    bool IsClicked() const {
        return IsHovered() && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    }

    void Draw() const {
        Color fill = IsHovered() ? SKYBLUE : LIGHTGRAY;
        DrawRectangleRec(rect, fill);
        DrawRectangleLinesEx(rect, 3, DARKGRAY);

        const int font_size = 30;
        int text_width = MeasureText(label, font_size);
        DrawText(label,
                 (int)(rect.x + (rect.width - text_width) / 2),
                 (int)(rect.y + (rect.height - font_size) / 2),
                 font_size, BLACK);
    }
};

class MainMenuScene : public Scene {
    const char* title = "<insert title here>";

    MenuButton start_button;
    MenuButton settings_button;

public:
    void Begin() override {
        const float button_width = 220;
        const float button_height = 60;
        const float center_x = (GetScreenWidth() - button_width) / 2;

        start_button    = { { center_x, 260, button_width, button_height }, "Start" };
        settings_button = { { center_x, 350, button_width, button_height }, "Settings" };
    }

    void End() override {}

    void Update() override {
        if (start_button.IsClicked() && GetSceneManager() != nullptr) {
            GetSceneManager()->SwitchScene(SCENE_GAME);
        }

        if (settings_button.IsClicked()) {
            // TODO: settings scene not implemented yet (placeholder)
            std::cout << "Settings not implemented yet" << std::endl;
        }
    }

    void Draw() override {
        const int title_size = 50;
        int title_width = MeasureText(title, title_size);
        DrawText(title, (GetScreenWidth() - title_width) / 2, 120, title_size, BLACK);

        start_button.Draw();
        settings_button.Draw();
    }
};

#endif
