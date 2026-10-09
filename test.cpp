#include "raylib.h"
#include "stdlib.h"
#include "stdio.h"

//constants
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define PLATFORM_WIDTH 100
#define PLATFORM_HEIGHT 20
#define PLAYER_WIDTH 108
#define PLAYER_HEIGHT 108
#define GRAVITY 700.0f
#define JUMP_FORCE 500.0f
#define MAX_PLATFORMS 10
#define SCROLL_SPEED 150.0f
#define WALL_WIDTH 100
#define MOVE_SPEED 200.0f
#define POWERUP_SIZE 30
#define POWERUP_DURATION 5.0f
#define BOOST_MULTIPLIER 1.5f
#define POWERUP_MIN_INTERVAL 15
#define POWERUP_MAX_INTERVAL 25


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

// Global variables
Platform platforms[MAX_PLATFORMS];
Player player;
float score = 0;
bool gameOver = false;
Texture2D playerTexture;
Rectangle sourceRec;
Rectangle WallOfFlesh = {0, 0, WALL_WIDTH, SCREEN_HEIGHT};
PowerUp powerUp;
float powerUpSpawnTimer = 0;
float jumpBoostTimeLeft = 0;
float speedBoostTimeLeft = 0;

// Function prototypes
void InitGame();
void UpdateGame(float deltaTime);
void DrawGame();
void DrawGameOverScreen();
void ResetPlatforms(Platform* platforms, int count);
void TrySpawnPowerUp();
void UpdatePowerUps(float deltaTime);

int main() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Platformer");

    playerTexture = LoadTexture("scott.png");
    sourceRec = (Rectangle){ 0, 0, (float)playerTexture.width/8, (float)playerTexture.height };

    InitGame();

    SetTargetFPS(60); 

    // int currentFrame = 0;
    // int frameCounter = 0;
    // int frameSpeed = 8;

    while (!WindowShouldClose()) { 
        float deltaTime = GetFrameTime();

        UpdateGame(deltaTime);

        // frameCounter++;
        // if (frameCounter >= (60/frameSpeed)) {
        //     frameCounter = 0;
        //     currentFrame++;
        //     if (currentFrame > 7) currentFrame = 0;

        //     sourceRec.x = (float)currentFrame * (float)playerTexture.width/8;
        // }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        if (gameOver) {
            DrawGameOverScreen();
        } else {
            DrawGame();
        }

        DrawTexturePro(playerTexture, sourceRec, (Rectangle){player.rect.x, player.rect.y, PLAYER_WIDTH, PLAYER_HEIGHT}, (Vector2){0, 0}, 0.0f, WHITE);

        EndDrawing();
    }

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

    score = 0;
    gameOver = false;
}

void UpdateGame(float deltaTime) {
    if (gameOver) return;

    float jumpForce = (jumpBoostTimeLeft > 0) ? JUMP_FORCE * BOOST_MULTIPLIER : JUMP_FORCE;
    float moveSpeed = (speedBoostTimeLeft > 0) ? MOVE_SPEED * BOOST_MULTIPLIER : MOVE_SPEED;

    if (IsKeyPressed(KEY_SPACE) && player.velocity.y == 0) {
        player.velocity.y = -jumpForce;
        player.onGround = false;
    }

    if (IsKeyDown(KEY_A)) {
        player.rect.x -= moveSpeed * deltaTime;
    }
    if (IsKeyDown(KEY_D)) {
        player.rect.x += moveSpeed * deltaTime;
    }

    player.velocity.y += GRAVITY * deltaTime;
    player.rect.y += player.velocity.y * deltaTime;
    player.rect.x += player.velocity.x * deltaTime;

    for (int i = 0; i < MAX_PLATFORMS; i++) {
        platforms[i].rect.x -= SCROLL_SPEED * deltaTime;

        // when goes past left edge respawns at right edge 
        if (platforms[i].rect.x + platforms[i].rect.width < 0) {
            ResetPlatforms(&platforms[i], 1);
            platforms[i].rect.x = SCREEN_WIDTH + (float)GetRandomValue(0, 200);
        }
    }
    score += SCROLL_SPEED * deltaTime * 0.1f;

    UpdatePowerUps(deltaTime);

    for (int i = 0; i < MAX_PLATFORMS; i++) {
        if (platforms[i].active && CheckCollisionRecs(player.rect, platforms[i].rect) && player.velocity.y > 0) {
            player.rect.y = platforms[i].rect.y - PLAYER_HEIGHT;
            player.velocity.y = 0;
            player.onGround = true;

            platforms[i].timeLeft -= deltaTime;
            if (platforms[i].timeLeft <= 0) {
                platforms[i].active = false; // Deactivate platform
            }
        }
    }

    if (player.rect.y > SCREEN_HEIGHT) {
        gameOver = true;
    }

    if (CheckCollisionRecs(player.rect, WallOfFlesh)) {
        gameOver = true;
    }
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

    DrawText(TextFormat("Score: %d", (int)score), 10, 10, 20, BLACK);

    int hudY = 35;
    if (jumpBoostTimeLeft > 0) {
        DrawText(TextFormat("Jump Boost: %.1fs", jumpBoostTimeLeft), 10, hudY, 20, DARKGREEN);
        hudY += 25;
    }
    if (speedBoostTimeLeft > 0) {
        DrawText(TextFormat("Speed Boost: %.1fs", speedBoostTimeLeft), 10, hudY, 20, DARKBLUE);
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

