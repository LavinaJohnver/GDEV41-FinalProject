#include "raylib.h"
#include "stdlib.h"
#include "stdio.h"
#include "math.h"

#include "scene_manager.hpp"
#include "main_menu_scene.hpp"
#include "player_animator.hpp"

//constants
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define PLATFORM_WIDTH 100
#define PLATFORM_HEIGHT 10
#define PLAYER_WIDTH 75
#define PLAYER_HEIGHT 75
#define GRAVITY 700.0f
#define JUMP_FORCE 400.0f
#define MAX_PLATFORMS 10
#define SCROLL_SPEED 150.0f
#define WALL_WIDTH 100
#define MOVE_SPEED 200.0f
#define POWERUP_SIZE 30
#define POWERUP_DURATION 5.0f
#define BOOST_MULTIPLIER 1.5f
#define POWERUP_MIN_INTERVAL 15
#define POWERUP_MAX_INTERVAL 25

//  GRAPPLING HOOK settings and vars
#define HOOK_SPEED 2000.0f      // how fast the hook head is
#define MAX_ROPE_LENGTH 520.0f  // max hook range
#define MIN_ROPE_LENGTH 40.0f   // min hook length
#define REEL_SPEED 150.0f       // rope pulled in (W) or let out (S) per second
#define SWING_CONTROL 450.0f    // how hard A/D push a swing along. Bigger = snappy, smaller = sluggishy
#define HOOK_RADIUS 6.0f        // how big the hook head looks
#define AIR_DRAG 0.8f           // slows sideways speed in the air, so the speed you get off a swing fades instead of lasting forever
#define GROUND_FRICTION 6.0f    // same thing, but while standing on a platform


typedef struct {
    Rectangle rect;
    float timeLeft;
    bool active;
} Platform;

typedef enum {
    POWERUP_JUMP,  // green box
    POWERUP_SPEED  // blue box
} PowerUpType;

typedef struct {
    Rectangle rect;
    PowerUpType type;
    bool active;
} PowerUp;

typedef struct {
    Rectangle rect;
    Vector2 velocity;
    bool onGround;
} Player;

// HOOK STATES
typedef enum {
    HOOK_IDLE,      // not shot yet and aiming crosshair is drawn
    HOOK_FLYING,    // hook moving w/ no effect on the player yet
    HOOK_ATTACHED   // hook hit a platform and the rope constrains the player
} HookState;

// Everything the hook has to remember between frames
//
// 3 HOOK STATES:
//   HOOK_IDLE      - ignore everything else in here
//   HOOK_FLYING    - position and velocity matter, attachedPlatform is -1
//   HOOK_ATTACHED  - attachedPlatform, anchorOffset and ropeLength matter velocity doesn't
typedef struct {
    HookState state;
    Vector2 position;       // where the hook head is on screen right now
    Vector2 velocity;       // how fast hook is in flight
    int attachedPlatform;   // which platform it's stuck in, -1 if not stuck
    Vector2 anchorOffset;   // where it stuck, counted from the platform's top-left corner. platform slides left per frame
    float ropeLength;       // how long the rope is; always kept between MIN_ROPE_LENGTH and MAX_ROPE_LENGTH
} Hook;

// Global variables
Platform platforms[MAX_PLATFORMS];
Player player;
float score = 0;
bool gameOver = false;
Rectangle WallOfFlesh = {0, 0, WALL_WIDTH, SCREEN_HEIGHT};
PowerUp powerUp;
float powerUpSpawnTimer = 0;
float jumpBoostTimeLeft = 0;
float speedBoostTimeLeft = 0;
Hook hook;
PlayerAnimator animator;   // owns everything about how Scott looks
                            // the flight pose survives letting go of the rope.

bool showHitboxes = false;   // debug view, toggled with F1

// Function prototypes
void InitGame();
void UpdateGame(float deltaTime);
void DrawGame();
void DrawGameOverScreen();
void ResetPlatforms(Platform* platforms, int count);
void TrySpawnPowerUp();
void UpdatePowerUps(float deltaTime);
void FireHook(Vector2 target);
void ReleaseHook();
void UpdateHook(float deltaTime);
void DrawHook();
void DrawHitboxes();

// vector helpers
static Vector2 V2Add(Vector2 a, Vector2 b)      { return (Vector2){ a.x + b.x, a.y + b.y }; }
static Vector2 V2Sub(Vector2 a, Vector2 b)      { return (Vector2){ a.x - b.x, a.y - b.y }; }
static Vector2 V2Scale(Vector2 a, float s)      { return (Vector2){ a.x * s, a.y * s }; }
static float   V2Dot(Vector2 a, Vector2 b)      { return a.x * b.x + a.y * b.y; }
static float   V2Length(Vector2 a)              { return sqrtf(a.x * a.x + a.y * a.y); }

static Vector2 V2Normalize(Vector2 a) {
    float len = V2Length(a);
    if (len < 0.0001f) return (Vector2){ 0, 0 };
    return (Vector2){ a.x / len, a.y / len };
}

static Vector2 PlayerCenter() {
    return (Vector2){ player.rect.x + PLAYER_WIDTH / 2.0f, player.rect.y + PLAYER_HEIGHT / 2.0f };
}

// Where the hook is stuck, in world coordinates. Platforms slide left every
// frame, so the saved offset is added to the platform's current position.
//
// Only when state is HOOK_ATTACHED, otherwise (0,0)
static Vector2 HookAnchor() {
    Rectangle r = platforms[hook.attachedPlatform].rect;
    return (Vector2){ r.x + hook.anchorOffset.x, r.y + hook.anchorOffset.y };
}

// Finds where the line from a to b first crosses the edge of rectangle r.
//
//   a, b : the line, usually one frame of hook travel
//   r    : the platform being checking
//   out  : gets the crossing point; left alone if there's nothing hit
//   returns true if line touches the rectangle at all
//
// All four edges are checked and the closest hit wins, so the hook sticks to the
// side it would really have hit first. CheckCollisionPointRec catches a line
// that starts inside the rectangle already
static bool SegmentHitsRect(Vector2 a, Vector2 b, Rectangle r, Vector2* out) {
    Vector2 tl = (Vector2){ r.x,           r.y };
    Vector2 tr = (Vector2){ r.x + r.width, r.y };
    Vector2 bl = (Vector2){ r.x,           r.y + r.height };
    Vector2 br = (Vector2){ r.x + r.width, r.y + r.height };

    Vector2 edgeStart[4] = { tl, tr, br, bl };
    Vector2 edgeEnd[4]   = { tr, br, bl, tl };

    bool found = false;
    float bestDist = 0;
    Vector2 bestPoint = (Vector2){ 0, 0 };

    for (int i = 0; i < 4; i++) {
        Vector2 p;
        if (CheckCollisionLines(a, b, edgeStart[i], edgeEnd[i], &p)) {
            float d = V2Length(V2Sub(p, a));
            if (!found || d < bestDist) { found = true; bestDist = d; bestPoint = p; }
        }
    }

    // the hook can also start a step already inside a platform
    if (!found && CheckCollisionPointRec(a, r)) { found = true; bestPoint = a; }

    if (found && out) *out = bestPoint;
    return found;
}

// ---------------------------------------------------------------- hook
// shoots hook from the player's center toward cursor + if hook is 
// already out it just gets replaced, so clicking again re-shoots 
// instead of doing nothing
void FireHook(Vector2 target) {
    Vector2 center = PlayerCenter();
    Vector2 dir = V2Normalize(V2Sub(target, center));
    if (dir.x == 0 && dir.y == 0) return;   // clicked exactly on yourself

    hook.state = HOOK_FLYING;
    hook.position = center;
    hook.velocity = V2Scale(dir, HOOK_SPEED);
    hook.attachedPlatform = -1;
}

// lets go of the rope and puts the hook away + keeps player.velocity
void ReleaseHook() {
    hook.state = HOOK_IDLE;
    hook.attachedPlatform = -1;
}

// moves the hook on a frame and applies the rope to the player
//
// This needs to run:
//   * after the player has moved, so it's fixing a position that's already final for this frame
//   * after the platforms have slid left, so HookAnchor() isn't reading stale positions
//   * before the landing checks, so they see the fixed position
//
// STATES:
//   HOOK_IDLE     - nothing
//   HOOK_FLYING   - moves the head along and looks for a platform. Sticks if it
//                   hits one, gives up once it runs out of rope or leaves the
//                   screen.
//   HOOK_ATTACHED - handles reeling, then applies the rope rule below
void UpdateHook(float deltaTime) {
    if (hook.state == HOOK_IDLE) return;

    if (hook.state == HOOK_FLYING) {
        Vector2 start = hook.position;
        Vector2 end = V2Add(start, V2Scale(hook.velocity, deltaTime));

        // test against every active platform, keeping the NEAREST hit
        int hitIndex = -1;
        float bestDist = 0;
        Vector2 hitPoint = end;

        for (int i = 0; i < MAX_PLATFORMS; i++) {
            if (!platforms[i].active) continue;
            Vector2 p;
            if (SegmentHitsRect(start, end, platforms[i].rect, &p)) {
                float d = V2Length(V2Sub(p, start));
                if (hitIndex < 0 || d < bestDist) {
                    hitIndex = i; bestDist = d; hitPoint = p; 
                }
            }
        }

        if (hitIndex >= 0) {
            hook.state = HOOK_ATTACHED;
            hook.position = hitPoint;
            hook.attachedPlatform = hitIndex;
            hook.anchorOffset = V2Sub(hitPoint, (Vector2){ platforms[hitIndex].rect.x,
                                                           platforms[hitIndex].rect.y });
            hook.ropeLength = V2Length(V2Sub(hitPoint, PlayerCenter()));
            if (hook.ropeLength > MAX_ROPE_LENGTH) hook.ropeLength = MAX_ROPE_LENGTH;
            if (hook.ropeLength < MIN_ROPE_LENGTH) hook.ropeLength = MIN_ROPE_LENGTH;
            return;
        }

        hook.position = end;

        // missed: give up once it outruns the rope or leaves the screen
        if (V2Length(V2Sub(hook.position, PlayerCenter())) > MAX_ROPE_LENGTH ||
            hook.position.x < 0 || hook.position.x > SCREEN_WIDTH ||
            hook.position.y < 0 || hook.position.y > SCREEN_HEIGHT) {
            ReleaseHook();
        }
        return;
    }

    // HOOK_ATTACHED
    // the platform may have crumbled out from under the hook
    if (hook.attachedPlatform < 0 || !platforms[hook.attachedPlatform].active) {
        ReleaseHook();
        return;
    }

    hook.position = HookAnchor();

    // reel in / let out
    if (IsKeyDown(KEY_W)) hook.ropeLength -= REEL_SPEED * deltaTime;
    if (IsKeyDown(KEY_S)) hook.ropeLength += REEL_SPEED * deltaTime;
    if (hook.ropeLength < MIN_ROPE_LENGTH) hook.ropeLength = MIN_ROPE_LENGTH;
    if (hook.ropeLength > MAX_ROPE_LENGTH) hook.ropeLength = MAX_ROPE_LENGTH;

    //  Swinging logic
    //
    //  2 steps and only while the rope is actually pulled tight:
    //    1. Position - shove the player back along the rope until they're
    //       exactly ropeLength from the anchor
    //
    //    2. Speed - kill the part pointing straight away from the anchor
    //       the rope pulls but can't push, so inward speed stays
    Vector2 center = PlayerCenter();
    Vector2 toAnchor = V2Sub(hook.position, center);
    float dist = V2Length(toAnchor);

    if (dist > hook.ropeLength && dist > 0.0001f) {
        Vector2 dir = V2Scale(toAnchor, 1.0f / dist);   // unit vector, player -> anchor

        // 1. position fix: shove them back onto the circle
        float overshoot = dist - hook.ropeLength;
        player.rect.x += dir.x * overshoot;
        player.rect.y += dir.y * overshoot;

        // 2. speed fix: only kill movement if heading away from the anchor
        float radial = V2Dot(player.velocity, dir);
        if (radial < 0) player.velocity = V2Sub(player.velocity, V2Scale(dir, radial));
    }
}

// draw the rope and the hook head
void DrawHook() {
    if (hook.state == HOOK_IDLE) {
        // small reticle so the player can see where they are aiming
        DrawCircleLinesV(GetMousePosition(), 8.0f, Fade(BLACK, 0.35f));
        return;
    }

    Vector2 center = PlayerCenter();
    Color ropeColor = (hook.state == HOOK_ATTACHED) ? DARKBROWN : GRAY;
    DrawLineEx(center, hook.position, 2.0f, ropeColor);
    DrawCircleV(hook.position, HOOK_RADIUS, (hook.state == HOOK_ATTACHED) ? ORANGE : GRAY);
}

// ---------------------------------------------------------------- debug view
// shows player collision box on top of the sprite; toggle with F1
void DrawHitboxes() {
    if (!showHitboxes) return;

    // the box every collision check actually uses
    DrawRectangleLinesEx(player.rect, 2.0f, LIME);

    // where the sprite actually lands, so you can see the feet line up
    DrawRectangleLinesEx(animator.SpriteDest(), 1.0f, Fade(MAGENTA, 0.6f));

    // the point the rope hangs from, and what the rope rule measures to
    DrawCircleV(PlayerCenter(), 3.0f, MAGENTA);
}

// The platformer gameplay, wrapped as a scene
class GameScene : public Scene {
public:
    void Begin() override {
        animator.Load();
        InitGame();
    }

    void End() override {}

    void Update() override {
        // debug toggle lives here rather than in UpdateGame, so it still works
        // while the game-over screen is up
        if (IsKeyPressed(KEY_F1)) showHitboxes = !showHitboxes;

        UpdateGame(GetFrameTime());
    }

    void Draw() override {
        if (gameOver) {
            DrawGameOverScreen();
            return;
        }

        DrawGame();
        animator.Draw();
        DrawHitboxes();   // on top of the sprite, so the box is never hidden by it
    }
};

int main() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Platformer");
    SetTargetFPS(60);

    SceneManager scene_manager;

    MainMenuScene main_menu_scene;
    main_menu_scene.SetSceneManager(&scene_manager);

    GameScene game_scene;
    game_scene.SetSceneManager(&scene_manager);

    scene_manager.RegisterScene(&main_menu_scene, SCENE_MAIN_MENU);
    scene_manager.RegisterScene(&game_scene, SCENE_GAME);

    scene_manager.SwitchScene(SCENE_MAIN_MENU);

    while (!WindowShouldClose()) {
        Scene* active_scene = scene_manager.GetActiveScene();

        BeginDrawing();
        ClearBackground(RAYWHITE);

        if (active_scene != nullptr) {
            active_scene->Update();
            active_scene->Draw();
        }

        EndDrawing();
    }

    Scene* active_scene = scene_manager.GetActiveScene();
    if (active_scene != nullptr) {
        active_scene->End();
    }

    ResourceManager::GetInstance()->UnloadAllTextures();

    CloseWindow();
    return 0;
}

void InitGame() {
    player.rect = (Rectangle){WALL_WIDTH + 100, SCREEN_HEIGHT - PLAYER_HEIGHT * 2, PLAYER_WIDTH, PLAYER_HEIGHT};
    player.velocity = (Vector2){0, 0};
    player.onGround = false;

    for (int i = 0; i < MAX_PLATFORMS; i++) {
        platforms[i].rect = (Rectangle){(float)GetRandomValue(WALL_WIDTH + PLAYER_WIDTH, SCREEN_WIDTH * 2), (float)GetRandomValue(0, SCREEN_HEIGHT - PLATFORM_HEIGHT), PLATFORM_WIDTH, PLATFORM_HEIGHT};
        platforms[i].timeLeft = 1.0f; 
        platforms[i].active = true;
    }

    // Guarantee a platform under the player at the start
    platforms[0].rect = (Rectangle){WALL_WIDTH + 80, SCREEN_HEIGHT - PLAYER_HEIGHT, PLATFORM_WIDTH, PLATFORM_HEIGHT};
    platforms[0].timeLeft = 1.0f;
    platforms[0].active = true;

    powerUp.active = false;
    powerUpSpawnTimer = (float)GetRandomValue(POWERUP_MIN_INTERVAL, POWERUP_MAX_INTERVAL);
    jumpBoostTimeLeft = 0;
    speedBoostTimeLeft = 0;

    animator.Reset();

    // clear the hook. InitGame() runs both when the scene starts (from
    // GameScene::Begin) and when R restarts, so a hook that was still attached
    // shouldn't carry over into either.
    hook.state = HOOK_IDLE;
    hook.attachedPlatform = -1;
    hook.ropeLength = 0;
    hook.position = (Vector2){ 0, 0 };
    hook.velocity = (Vector2){ 0, 0 };
    hook.anchorOffset = (Vector2){ 0, 0 };

    score = 0;
    gameOver = false;
}

void UpdateGame(float deltaTime) {
    if (gameOver) return;

    float jumpForce = (jumpBoostTimeLeft > 0) ? JUMP_FORCE * BOOST_MULTIPLIER : JUMP_FORCE;
    float moveSpeed = (speedBoostTimeLeft > 0) ? MOVE_SPEED * BOOST_MULTIPLIER : MOVE_SPEED;

    // --- hook input
    // Press to fire, hold to stay attached, let go to drop.
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) FireHook(GetMousePosition());
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) ReleaseHook();

    if (IsKeyPressed(KEY_SPACE)) {
        animator.OnJump();   // ends the hook flight and straightens him up

        if (hook.state == HOOK_ATTACHED) {
            // jump off the rope: keep the swing momentum + a small jump
            ReleaseHook();
            player.velocity.y -= jumpForce * 0.6f;
        } else if (player.velocity.y == 0) {
            player.velocity.y = -jumpForce;
            player.onGround = false;
        }
    }

    // Movement depends on the state:
    // if on foot, A/D move the box directly
    // if on a rope they add speed instead
    if (hook.state == HOOK_ATTACHED) {
        // while swinging, A/D pump the pendulum instead of teleporting you sideways
        if (IsKeyDown(KEY_A)) player.velocity.x -= SWING_CONTROL * deltaTime;
        if (IsKeyDown(KEY_D)) player.velocity.x += SWING_CONTROL * deltaTime;
    } else {
        if (IsKeyDown(KEY_A)) player.rect.x -= moveSpeed * deltaTime;
        if (IsKeyDown(KEY_D)) player.rect.x += moveSpeed * deltaTime;

        // momentum carried out of a swing bleeds off instead of lasting forever
        float drag = player.onGround ? GROUND_FRICTION : AIR_DRAG;
        player.velocity.x -= player.velocity.x * drag * deltaTime;
        if (fabsf(player.velocity.x) < 1.0f) player.velocity.x = 0;
    }

    player.velocity.y += GRAVITY * deltaTime;
    player.rect.y += player.velocity.y * deltaTime;
    player.rect.x += player.velocity.x * deltaTime;

    for (int i = 0; i < MAX_PLATFORMS; i++) {
        platforms[i].rect.x -= SCROLL_SPEED * deltaTime;

        // when goes past left edge respawns at right edge 
        if (platforms[i].rect.x + platforms[i].rect.width < 0) {
            // don't stay hooked to a platform that is about to teleport
            if (hook.state == HOOK_ATTACHED && hook.attachedPlatform == i) ReleaseHook();
            ResetPlatforms(&platforms[i], 1);
            platforms[i].rect.x = SCREEN_WIDTH + (float)GetRandomValue(0, 200);
        }
    }
    score += SCROLL_SPEED * deltaTime * 0.1f;

    UpdatePowerUps(deltaTime);

    // the rope is solved after the platforms have scrolled, so the anchor is current
    UpdateHook(deltaTime);

    player.onGround = false;

    // While the rope is attached he swings straight past platforms instead of
    // landing on them. Let go or jump and landing works again.
    if (hook.state != HOOK_ATTACHED) {
        for (int i = 0; i < MAX_PLATFORMS; i++) {
            if (platforms[i].active && CheckCollisionRecs(player.rect, platforms[i].rect) && player.velocity.y > 0) {
                player.rect.y = platforms[i].rect.y - PLAYER_HEIGHT;
                player.velocity.y = 0;
                player.onGround = true;
                animator.OnLand();   // ends the hook flight and straightens him up

                platforms[i].timeLeft -= deltaTime;
                if (platforms[i].timeLeft <= 0) {
                    platforms[i].active = false; // Deactivate platform
                }
            }
        }
    }

    // make it so player scrolls left w the platforms
    if (player.onGround) {
        player.rect.x -= SCROLL_SPEED * deltaTime;
    }

    if (player.rect.y > SCREEN_HEIGHT) {
        gameOver = true;
    }

    if (CheckCollisionRecs(player.rect, WallOfFlesh)) {
        gameOver = true;
    }

    // Send required stuff to animator
    PlayerState view;
    view.rect         = player.rect;                    // hitbox
    view.velocity     = player.velocity;                // velocity
    view.onGround     = player.onGround;                // on ground
    view.hookAttached = (hook.state == HOOK_ATTACHED);  // if hook hit smth
    view.hookPosition = hook.position;                  // where the hook is

    animator.Update(deltaTime, view);
}

void ResetPlatforms(Platform* platforms, int count) {
    for (int i = 0; i < count; i++) {
        platforms[i].rect = (Rectangle){(float)GetRandomValue(0, SCREEN_WIDTH - PLATFORM_WIDTH), (float)GetRandomValue(0, SCREEN_HEIGHT - PLATFORM_HEIGHT), PLATFORM_WIDTH, PLATFORM_HEIGHT};
        platforms[i].timeLeft = 3.0f; 
        platforms[i].active = true;
    }
}

// Places a random power-up on top of a platform in the right half of the screen (or just past it)
void TrySpawnPowerUp() {
    int candidates[MAX_PLATFORMS];
    int count = 0;
    for (int i = 0; i < MAX_PLATFORMS; i++) {
        if (platforms[i].active && platforms[i].rect.x >= SCREEN_WIDTH / 2 && platforms[i].rect.y >= POWERUP_SIZE) {
            candidates[count++] = i;
        }
    }

    if (count == 0) {
        powerUpSpawnTimer = 1.0f; // no good platform right now, try again shortly
        return;
    }

    Rectangle p = platforms[candidates[GetRandomValue(0, count - 1)]].rect;
    powerUp.rect = (Rectangle){p.x + (p.width - POWERUP_SIZE) / 2, p.y - POWERUP_SIZE, POWERUP_SIZE, POWERUP_SIZE};
    powerUp.type = (GetRandomValue(0, 1) == 0) ? POWERUP_JUMP : POWERUP_SPEED;
    powerUp.active = true;
}

void UpdatePowerUps(float deltaTime) {
    if (jumpBoostTimeLeft > 0) jumpBoostTimeLeft -= deltaTime;
    if (speedBoostTimeLeft > 0) speedBoostTimeLeft -= deltaTime;

    if (!powerUp.active) {
        powerUpSpawnTimer -= deltaTime;
        if (powerUpSpawnTimer <= 0) {
            TrySpawnPowerUp();
        }
        return;
    }

    // scrolls with the platforms
    powerUp.rect.x -= SCROLL_SPEED * deltaTime;

    if (CheckCollisionRecs(player.rect, powerUp.rect)) {
        if (powerUp.type == POWERUP_JUMP) {
            jumpBoostTimeLeft = POWERUP_DURATION;
        } else {
            speedBoostTimeLeft = POWERUP_DURATION;
        }
        powerUp.active = false;
    } else if (powerUp.rect.x + powerUp.rect.width < 0) {
        powerUp.active = false; // missed it
    }

    if (!powerUp.active) {
        powerUpSpawnTimer = (float)GetRandomValue(POWERUP_MIN_INTERVAL, POWERUP_MAX_INTERVAL);
    }
}

void DrawGame() {
    for (int i = 0; i < MAX_PLATFORMS; i++) {
        if (platforms[i].active) {
            DrawRectangleRec(platforms[i].rect, BLUE);
        }
    }

    if (powerUp.active) {
        DrawRectangleRec(powerUp.rect, (powerUp.type == POWERUP_JUMP) ? GREEN : DARKBLUE);
        DrawRectangleLinesEx(powerUp.rect, 2, BLACK);
    }

    DrawRectangleRec(WallOfFlesh, RED);

    DrawHook();

    DrawText(TextFormat("Score: %d", (int)score), 10, 10, 20, BLACK);

    int hudY = 35;
    if (jumpBoostTimeLeft > 0) {
        DrawText(TextFormat("Jump Boost: %.1fs", jumpBoostTimeLeft), 10, hudY, 20, DARKGREEN);
        hudY += 25;
    }
    if (speedBoostTimeLeft > 0) {
        DrawText(TextFormat("Speed Boost: %.1fs", speedBoostTimeLeft), 10, hudY, 20, DARKBLUE);
        hudY += 25;
    }
    if (hook.state == HOOK_ATTACHED) {
        DrawText(TextFormat("Rope: %.0f  (W reel in / S let out)", hook.ropeLength), 10, hudY, 20, DARKBROWN);
    }
}

void DrawGameOverScreen() {
    DrawText("Game Over!", SCREEN_WIDTH/2 - MeasureText("Game Over!", 40)/2, SCREEN_HEIGHT/2 - 20, 40, RED);
    DrawText(TextFormat("Final Score: %d", (int)score), SCREEN_WIDTH/2 - MeasureText(TextFormat("Final Score: %d", (int)score), 20)/2, SCREEN_HEIGHT/2 + 30, 20, BLACK);
    DrawText("Press R to Restart", SCREEN_WIDTH/2 - MeasureText("Press R to Restart", 20)/2, SCREEN_HEIGHT/2 + 60, 20, BLACK);

    if (IsKeyPressed(KEY_R)) {
        InitGame();
    }
}

