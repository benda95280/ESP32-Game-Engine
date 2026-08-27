#pragma once

#include <Arduino.h> 
#include <vector>
#include <functional> 
#include "Scene.h" 

#define MAX_SCENES 5

// Forward declarations
class InputManager;
class Renderer;
class SceneTransition;

// Define the logger type directly to break circular dependency with EDGE.h
using EDGELogger = std::function<void(const char* message)>;

using SceneFactoryFunction = std::function<Scene*(void* configData)>;


class SceneManager {
public:
    static constexpr size_t MAX_SCENE_NAME_LEN = 32;

    SceneManager(); 
    ~SceneManager();
    void processSceneChanges(); 

    void setInputManager(InputManager* manager);
    void setLogger(EDGELogger logger);

    bool registerScene(const char* name, SceneFactoryFunction factory);

    bool setCurrentScene(const char* sceneName, void* configData = nullptr, SceneTransition* transition = nullptr); 
    bool pushScene(const char* sceneName, void* configData = nullptr, SceneTransition* transition = nullptr);    
    bool popScene();

    void requestSetCurrentScene(const char* sceneName, void* configData = nullptr, SceneTransition* transition = nullptr);
    void requestPushScene(const char* sceneName, void* configData = nullptr, SceneTransition* transition = nullptr);

    void update(unsigned long dt);
    void draw(Renderer& rendererRef); 
    Scene* getCurrentScene() const;
    const char* getCurrentSceneName() const;
    const char* getPreviousSceneName() const;

    SceneFactoryFunction getFactoryByName(const char* name) const;
    std::vector<const char*> getRegisteredSceneNames() const;

    bool isSceneChangePending() const;
    const char* getPendingSceneName() const;
    void* getPendingConfigData() const;
    bool getPendingReplaceStack() const;
    void clearPendingSceneChange();

    bool isTransitioning() const { return _activeTransition != nullptr; }
    bool shouldBlockInput() const;


private:
    Scene* sceneStack[MAX_SCENES] = {nullptr};
    int sceneCount = 0;
    char _sceneNameStack[MAX_SCENES][MAX_SCENE_NAME_LEN] = {}; 
    char _previousSceneName[MAX_SCENE_NAME_LEN] = {};

    InputManager* inputManager = nullptr;
    EDGELogger _logger;

    // Fast vector of static/literal const char* keys
    std::vector<std::pair<const char*, SceneFactoryFunction>> _sceneFactories;

    char _pendingNextSceneName[MAX_SCENE_NAME_LEN] = {};
    void* _pendingConfigData = nullptr;
    bool _pendingReplaceStack = true;
    bool _pendingSceneChange = false;
    SceneTransition* _pendingTransition = nullptr;

    Scene* _outgoingScene = nullptr;
    SceneTransition* _activeTransition = nullptr;

    Scene* setupNewScene(const char* sceneName, void* configData);
    Scene* createSceneByName(const char* sceneName, void* configData); 
    void clearStack();
    void cleanupOutgoingScene();
    void forceCleanupTransition();
};