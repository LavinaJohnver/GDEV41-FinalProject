#ifndef SCENE_MANAGER
#define SCENE_MANAGER

#include <raylib.h>

#include <iostream>
#include <string>
#include <unordered_map>

// Scene IDs used when registering / switching scenes
enum SceneId {
    SCENE_MAIN_MENU = 0,
    SCENE_GAME = 1,
};

class SceneManager;

// Base class that all scenes inherit
class Scene {
    SceneManager* scene_manager = nullptr;
public:
    virtual ~Scene() {}

    // Begins the scene. This is where you can load resources
    virtual void Begin() = 0;

    // Ends the scene. This is where you can unload resources
    virtual void End() = 0;

    // Updates scene's state (physics, input, etc.)
    virtual void Update() = 0;

    // Draws the scene's current state
    virtual void Draw() = 0;

    void SetSceneManager(SceneManager* scene_manager) {
        this->scene_manager = scene_manager;
    }

    SceneManager* GetSceneManager() {
        return scene_manager;
    }
};

class SceneManager {
    // Mapping between a scene ID and a reference to the scene
    std::unordered_map<int, Scene*> scenes;

    // Current active scene
    Scene* active_scene = nullptr;

public:
    void RegisterScene(Scene* scene, int scene_id) {
        scenes[scene_id] = scene;
    }

    void UnregisterScene(int scene_id) {
        scenes.erase(scene_id);
    }

    void SwitchScene(int scene_id) {
        if (scenes.find(scene_id) == scenes.end()) {
            std::cout << "Scene ID not found" << std::endl;
            return;
        }

        if (active_scene != nullptr) {
            active_scene->End();
        }

        std::cout << "Moved to Scene " << scene_id << std::endl;

        active_scene = scenes[scene_id];
        active_scene->Begin();
    }

    Scene* GetActiveScene() {
        return active_scene;
    }
};

// Resource manager implemented as a singleton
class ResourceManager {
    std::unordered_map<std::string, Texture> textures;

    ResourceManager() {}

public:
    ResourceManager(const ResourceManager&) = delete;
    void operator=(const ResourceManager&) = delete;

    static ResourceManager* GetInstance() {
        static ResourceManager instance;
        return &instance;
    }

    Texture GetTexture(const std::string& path) {
        if (textures.find(path) == textures.end()) {
            std::cout << "Loaded " << path << " from Disk" << std::endl;
            textures[path] = LoadTexture(path.c_str());
        }

        return textures[path];
    }

    void UnloadAllTextures() {
        for (auto& it : textures) {
            UnloadTexture(it.second);
        }

        textures.clear();
    }
};

#endif
