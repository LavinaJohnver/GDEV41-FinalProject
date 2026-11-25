#ifndef UI_HPP
#define UI_HPP

#include <raylib.h>

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

// Horizontal slider for a value between 0 and 1.
// Click anywhere on the bar to jump there, or hold and drag.
struct Slider {
    Rectangle rect;      // the bar itself; the label is drawn above it
    const char* label;
    bool dragging = false;

    // Updates the value from the mouse. Returns true if it changed this frame.
    bool Update(float* value) {
        Vector2 mouse = GetMousePosition();

        // grab area is a bit taller than the bar so it's easy to hit
        Rectangle grab = { rect.x - 10, rect.y - 10, rect.width + 20, rect.height + 20 };
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, grab)) {
            dragging = true;
        }
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            dragging = false;
        }
        if (!dragging) return false;

        float t = (mouse.x - rect.x) / rect.width;
        if (t < 0) t = 0;
        if (t > 1) t = 1;

        bool changed = (t != *value);
        *value = t;
        return changed;
    }

    void Draw(float value) const {
        const int font_size = 24;
        DrawText(label, (int)rect.x, (int)rect.y - font_size - 8, font_size, BLACK);

        const char* percent = TextFormat("%d%%", (int)(value * 100.0f + 0.5f));
        DrawText(percent,
                 (int)(rect.x + rect.width - MeasureText(percent, font_size)),
                 (int)rect.y - font_size - 8, font_size, DARKGRAY);

        // track, filled part, then the knob
        DrawRectangleRec(rect, LIGHTGRAY);
        DrawRectangleRec({ rect.x, rect.y, rect.width * value, rect.height }, SKYBLUE);
        DrawRectangleLinesEx(rect, 2, DARKGRAY);

        bool hot = dragging || CheckCollisionPointRec(GetMousePosition(), rect);
        Vector2 knob = { rect.x + rect.width * value, rect.y + rect.height / 2 };
        DrawCircleV(knob, rect.height * 0.9f, hot ? BLUE : DARKBLUE);
    }
};

#endif
