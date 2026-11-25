#ifndef PLAYER_ANIMATOR
#define PLAYER_ANIMATOR

#include <raylib.h>
#include <math.h>

#include "scene_manager.hpp"   // for ResourceManager

// how the sprites are fitted to the hitbox
#define SPRITE_SCALE 1.0f        // extra multiplier on every pose, 1.0 = leave alone
#define SPRITE_FEET_GAP 6.0f     // 6 lines up w the foot down frames while other frames are higher
#define SPRITE_NUDGE_X 0.0f      // + moves him right
#define SPRITE_NUDGE_Y 0.0f      // + moves him down

// images were exported at different sizes, so each needs its own scale or he changes size between poses
// how tall Scott's body is drawn, as a multiple of the hitbox height; idk why but 1.111 works best trust
#define CHAR_HEIGHT_RATIO 1.111f
#define BODY_PX_RUN 120.0f        // body height inside one scott.png frame
#define BODY_PX_IDLE 59.0f
#define BODY_PX_JUMP 60.0f
#define BODY_PX_FLIGHT 59.0f      

// run cycle (scottrun.png)
#define ANIM_FRAMES 8             
#define ANIM_FPS 12.0f           
#define ANIM_IDLE_FRAME 1         // run frame to resume from when movement restarts

// idle anims (scottidle.png)
#define IDLE_FRAMES 8
#define IDLE_FPS 8.0f            // slower bc he's not bobbing that fast
#define IDLE_FRAME_PITCH 39.0f   // distance between frame left edges
#define IDLE_FRAME_X0 (-2.0f)    // frame is a bit clipped, accounted for
#define IDLE_FRAME_W 36.0f       // widest frame's width

// flight pose (scottflight.png)
#define FLIGHT_BACK_OFFSET 90.0f // grapple comes out of back (ass)

// which sprite shows
typedef enum {
    POSE_IDLE,   // standing still
    POSE_RUN,    // run cycle
    POSE_JUMP,   // airborne via jumping
    POSE_FLIGHT  // airborne via grappling hook hitting something
} PlayerPose;

// stuff animator needs to know
struct PlayerState {
    Rectangle rect;          // hitbox
    Vector2   velocity;
    bool      onGround;
    bool      hookAttached;  // the hook hit something
    Vector2   hookPosition;  // where the hook hit
};

class PlayerAnimator {
    // the four sprites
    Texture2D runSheet;
    Texture2D idleStrip;
    Texture2D jumpImage;
    Texture2D flightImage;

    Rectangle runSrc;        // window into the run sheet for the current frame

    // cycle state
    int   runFrame;
    float runTimer;
    int   idleFrame;
    float idleTimer;

    // pose state
    PlayerPose pose;
    bool  facingRight;
    bool  moving;
    bool  hookFlight;        // change when hook hits + cleared by OnJump/OnLand
    float rotation;          // in deg, frozen when rope lets go because funny

    PlayerState last;        // last state handed to Update, so Draw can use it

public:
    PlayerAnimator() { Reset(); }

    void Load() {
        ResourceManager* rm = ResourceManager::GetInstance();
        runSheet = rm->GetTexture("scottrun.png");
        idleStrip = rm->GetTexture("scottidle.png");
        jumpImage = rm->GetTexture("scottjump.png");
        flightImage = rm->GetTexture("scottflight.png");
        runSrc = (Rectangle){ 0, 0, (float)runSheet.width/(float)ANIM_FRAMES, (float)runSheet.height };
    }

    void Reset() {
        runFrame = ANIM_IDLE_FRAME;
        runTimer = 0.0f;
        idleFrame = 0;
        idleTimer = 0.0f;
        pose = POSE_IDLE;
        facingRight = true;
        moving = false;
        hookFlight = false;
        rotation = 0.0f;
    }

    // if jumping, switch pose and reset rotation
    void OnJump() { hookFlight = false; rotation = 0.0f; }

    // if landed, switch pose and reset rotation
    void OnLand() { hookFlight = false; rotation = 0.0f; }

    void Update(float deltaTime, const PlayerState& state) {
        last = state;

        // which direction is being faced (left or right)
        if (IsKeyDown(KEY_A)) facingRight = false;
        if (IsKeyDown(KEY_D)) facingRight = true;

        // run pose only while A or D is actually held down
        moving = IsKeyDown(KEY_A) || IsKeyDown(KEY_D);

        // switch only when hook hits
        if (state.hookAttached) {
            hookFlight = true;

            Vector2 centre = Centre();
            float dx = state.hookPosition.x - centre.x;
            float dy = state.hookPosition.y - centre.y;
            if (dx * dx + dy * dy > 0.0001f) {
                // atan2 gives the rope's angle measured from right, clockwise,
                // because y grows downwards on screen, FLIGHT_BACK_OFFSET
                // turns the sprite from its back-up rest pose to back-toward-hook/anchor
                rotation = atan2f(dy, dx) * RAD2DEG + FLIGHT_BACK_OFFSET;
            }
        }

        // prioritize airborne
        pose = (!state.onGround) ? (hookFlight ? POSE_FLIGHT : POSE_JUMP)
             : (moving ? POSE_RUN : POSE_IDLE);

        StepIdleCycle(deltaTime);
        StepRunCycle(deltaTime);

        runSrc.x = runFrame * RunFrameWidth();
        runSrc.width = facingRight ? RunFrameWidth() : -RunFrameWidth();
    }

    void Draw() const {
        switch (pose) {
            case POSE_RUN:
                DrawTexturePro(runSheet, runSrc, RunDest(), (Vector2){0, 0}, 0.0f, WHITE);
                break;

            case POSE_JUMP:
                DrawTexturePro(jumpImage, FullFrameSrc(jumpImage), JumpDest(),
                               (Vector2){0, 0}, 0.0f, WHITE);
                break;

            case POSE_FLIGHT: {
                // make dest.x/y the pivot, so he turns around the hook
                Rectangle box = FlightDest();
                Vector2 pivot = { box.width * 0.5f, box.height * 0.5f };
                Rectangle dest = { box.x + pivot.x, box.y + pivot.y, box.width, box.height };

                DrawTexturePro(flightImage, FullFrameSrc(flightImage), dest, pivot,
                               rotation, WHITE);
                break;
            }

            default:
                DrawTexturePro(idleStrip, IdleFrameSrc(), IdleDest(),
                               (Vector2){0, 0}, 0.0f, WHITE);
                break;
        }
    }

    // for the debug and tests
    PlayerPose Pose() const { return pose; }
    int   RunFrame() const { return runFrame; }
    int   IdleFrame() const { return idleFrame; }
    bool  FacingRight() const { return facingRight; }
    bool  InHookFlight() const { return hookFlight; }
    float Rotation() const { return rotation; }

    // where the picture lands, whichever pose it is
    Rectangle SpriteDest() const {
        switch (pose) {
            case POSE_RUN:    return RunDest();
            case POSE_JUMP:   return JumpDest();
            case POSE_FLIGHT: return FlightDest();
            default:          return IdleDest();
        }
    }

private:
    Vector2 Centre() const {
        return (Vector2){ last.rect.x + last.rect.width  * 0.5f,
                          last.rect.y + last.rect.height * 0.5f };
    }

    // how tall his body should come out this frame, from the hitbox
    float BodyOnScreen() const { 
        return last.rect.height * CHAR_HEIGHT_RATIO; 
    }

    float RunFrameWidth() const {
        return (float)runSheet.width / (float)ANIM_FRAMES;
    }

    void StepRunCycle(float deltaTime) {
        if (!moving) {
            // the idle art takes over on screen; this just parks the run cycle so it resumes from a planted-foot frame
            runFrame = ANIM_IDLE_FRAME;
            runTimer = 0.0f;
            return;
        }

        // while loop rather than an if, so long frames catches the animation up instead of leaving it behind
        const float secondsPerFrame = 1.0f / ANIM_FPS;
        runTimer += deltaTime;
        while (runTimer >= secondsPerFrame) {
            runTimer -= secondsPerFrame;
            runFrame = (runFrame + 1) % ANIM_FRAMES;
        }
    }

    void StepIdleCycle(float deltaTime) {
        // idle only runs while he is actually standing there but if
        // interrupted it, and it restarts from frame 0 next time he stops
        if (pose != POSE_IDLE) {
            idleFrame = 0;
            idleTimer = 0.0f;
            return;
        }

        const float secondsPerIdleFrame = 1.0f / IDLE_FPS;
        idleTimer += deltaTime;
        while (idleTimer >= secondsPerIdleFrame) {
            idleTimer -= secondsPerIdleFrame;
            idleFrame = (idleFrame + 1) % IDLE_FRAMES;
        }
    }

    // whole image as source rectangle, flip when he's facing left
    Rectangle FullFrameSrc(Texture2D tex) const {
        float w = (float)tex.width;
        return (Rectangle){ 0, 0, facingRight ? w : -w, (float)tex.height };
    }

    // window into the idle strip for whichever frame is showing
    Rectangle IdleFrameSrc() const {
        float x = fmaxf(0.0f, IDLE_FRAME_X0 + IDLE_FRAME_PITCH * (float)idleFrame);
        return (Rectangle){ x, 0,
                            facingRight ? IDLE_FRAME_W : -IDLE_FRAME_W,
                            (float)idleStrip.height };
    }

    // where the run frame goes
    Rectangle RunDest() const {
        // width goes negative when flipped, so size off the magnitude
        float frameW = fabsf(runSrc.width);
        float frameH = runSrc.height;

        if (frameW <= 0.0f || frameH <= 0.0f) return last.rect;

        float scale = (BodyOnScreen() / BODY_PX_RUN) * SPRITE_SCALE;
        float drawW = frameW * scale;
        float drawH = frameH * scale;

        // every frame has empty space under the feet which is different depending on which frame
        float feetGap = SPRITE_FEET_GAP * scale;

        return (Rectangle){
            last.rect.x + (last.rect.width - drawW) * 0.5f + SPRITE_NUDGE_X,
            last.rect.y + last.rect.height - drawH + feetGap + SPRITE_NUDGE_Y,
            drawW,
            drawH
        };
    }

    // fits a whole-image pose onto the hitbox + scale so his body matches the
    // hitbox height, center it, and stand him on the bottom
    Rectangle PoseDest(Texture2D tex, float bodyPx) const {
        float w = (float)tex.width;
        float h = (float)tex.height;

        if (w <= 0.0f || h <= 0.0f || bodyPx <= 0.0f) return last.rect;

        float scale = (BodyOnScreen() / bodyPx) * SPRITE_SCALE;
        float drawW = w * scale;
        float drawH = h * scale;

        return (Rectangle){
            last.rect.x + (last.rect.width - drawW) * 0.5f + SPRITE_NUDGE_X,
            last.rect.y + last.rect.height - drawH + SPRITE_NUDGE_Y,
            drawW,
            drawH
        };
    }

    // idle pose is a long strip so its dest is sized from 1 frame
    Rectangle IdleDest() const {
        Texture2D oneFrame = idleStrip;
        oneFrame.width = (int)IDLE_FRAME_W;
        return PoseDest(oneFrame, BODY_PX_IDLE);
    }

    Rectangle JumpDest() const { return PoseDest(jumpImage, BODY_PX_JUMP); }

    // flight pose isn't standing up, so center on hitbox rather than orientation of character
    Rectangle FlightDest() const {
        float w = (float)flightImage.width;
        float h = (float)flightImage.height;

        if (w <= 0.0f || h <= 0.0f) return last.rect;

        float scale = (BodyOnScreen() / BODY_PX_FLIGHT) * SPRITE_SCALE;
        float drawW = w * scale;
        float drawH = h * scale;

        return (Rectangle){
            last.rect.x + (last.rect.width  - drawW) * 0.5f + SPRITE_NUDGE_X,
            last.rect.y + (last.rect.height - drawH) * 0.5f + SPRITE_NUDGE_Y,
            drawW,
            drawH
        };
    }
};

#endif
