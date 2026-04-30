#include "SceneManager.h" 
#include "Scene.h"
#include "SceneTransition.h"
#include "InputManager.h"
#include "Renderer.h"
#include <Arduino.h>         
#include <vector>            
#include <string>

SceneManager::SceneManager() : _logger(nullptr) {
    // Constructor is now empty. Dependencies will be injected.
}

SceneManager::~SceneManager() {
    forceCleanupTransition();
    clearStack(); 
}

void SceneManager::setLogger(EDGELogger logger) {
    _logger = logger;
    // When the SceneManager gets its logger, it passes it down to the base Scene class static setter
    Scene::setMasterLogger(logger);
}


// --- NEW GETTER IMPLEMENTATIONS ---
bool SceneManager::isSceneChangePending() const { return _pendingSceneChange; }
String SceneManager::getPendingSceneName() const { return _pendingNextSceneName; }
void* SceneManager::getPendingConfigData() const { return _pendingConfigData; }
bool SceneManager::getPendingReplaceStack() const { return _pendingReplaceStack; }
String SceneManager::getPreviousSceneName() const { return _previousSceneName; }
// --- END NEW GETTER IMPLEMENTATIONS ---

void SceneManager::processSceneChanges() {
    if (!_pendingSceneChange) {
        return;
    }

    if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[SCENES] Processing scene change request. Target: %s, Replace: %s", _pendingNextSceneName.c_str(), _pendingReplaceStack ? "true" : "false"); _logger(buf); }

    String nameToSet = _pendingNextSceneName;
    bool replace = _pendingReplaceStack;
    void* configPtr = _pendingConfigData;
    SceneTransition* transition = _pendingTransition;

    if (nameToSet != "UNKNOWN" && nameToSet != "") {
        if (replace) {
            setCurrentScene(nameToSet, configPtr, transition);
        } else {
            pushScene(nameToSet, configPtr, transition);
        }
    } else {
        if (_logger) _logger("[SCENES] Scene change NOT processed. Target name was invalid.");
    }

    clearPendingSceneChange();
}

void SceneManager::setInputManager(InputManager* manager) {
    inputManager = manager;
}


bool SceneManager::registerScene(const String& name, SceneFactoryFunction factory) {
    if (!factory) {
        if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[SCENES] Tried to register a null factory for Scene '%s'", name.c_str()); _logger(buf); }
        return false;
    }
    if (name.isEmpty()) {
        if (_logger) _logger("[SCENES] Scene name cannot be empty for registration.");
        return false;
    }

    bool replacedFactory = _sceneFactories.count(name.c_str());

    if (replacedFactory) {
        if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[SCENES] Overwriting factory registration for Scene '%s'.", name.c_str()); _logger(buf); }
    }
    
    _sceneFactories[name.c_str()] = factory;

    if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[SCENES] Registered Scene '%s' with its factory.", name.c_str()); _logger(buf); }
    return true;
}

void SceneManager::requestSetCurrentScene(const String& sceneName, void* configData, SceneTransition* transition) {
    if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[SCENES] Requesting to SET current scene to '%s'", sceneName.c_str()); _logger(buf); }
    
    _pendingNextSceneName = sceneName;
    _pendingConfigData = configData;
    _pendingTransition = transition;
    _pendingReplaceStack = true;
    _pendingSceneChange = true;
}

void SceneManager::requestPushScene(const String& sceneName, void* configData, SceneTransition* transition) {
    if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[SCENES] Requesting to PUSH scene '%s'", sceneName.c_str()); _logger(buf); }

    _pendingNextSceneName = sceneName;
    _pendingConfigData = configData;
    _pendingTransition = transition;
    _pendingReplaceStack = false;
    _pendingSceneChange = true;
}

void SceneManager::clearPendingSceneChange() {
    _pendingSceneChange = false;
    _pendingNextSceneName = "";
    if (_pendingConfigData) {
        _pendingConfigData = nullptr;
    }
    _pendingTransition = nullptr;
}

void SceneManager::clearStack() {
    if (_logger) _logger("[SCENES] Clearing scene stack.");
    for (int i = sceneCount - 1; i >= 0; --i) {
        if (sceneStack[i]) {
            if (inputManager) {
                inputManager->unregisterAllListenersForScene(sceneStack[i]);
                inputManager->clearDeferredActionsForScene(sceneStack[i]);
            }
            if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[SCENES] Deleting scene '%s' (%p) from stack index %d", _sceneNameStack[i].c_str(), sceneStack[i], i); _logger(buf); }
            delete sceneStack[i];
            sceneStack[i] = nullptr;
            _sceneNameStack[i] = "";
        }
    }
    sceneCount = 0;
}

Scene* SceneManager::createSceneByName(const String& sceneName, void* configData) {
    if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[SCENES] Attempting to create scene '%s' using factory.", sceneName.c_str()); _logger(buf); }
    
    auto it = _sceneFactories.find(sceneName.c_str());
    if (it == _sceneFactories.end()) {
        if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[SCENES] No factory registered for Scene '%s'!", sceneName.c_str()); _logger(buf); }
        return nullptr;
    }

    SceneFactoryFunction& factory = it->second;
    Scene* newScene = factory(configData); 

    if (newScene == nullptr) {
        if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[SCENES] Factory for Scene '%s' returned null!", sceneName.c_str()); _logger(buf); }
        return nullptr;
    }

    if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[SCENES] Factory created scene '%s' at %p, calling generic init...", sceneName.c_str(), newScene); _logger(buf); }
    // newScene->init(); // init() is now called by the factory function in Main.cpp

    return newScene;
}


bool SceneManager::setCurrentScene(const String& sceneName, void* configData, SceneTransition* transition) { 
    if (!inputManager) { 
        if (_logger) _logger("[SCENES] InputManager is null in setCurrentScene.");
        return false; 
    }
    if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[SCENES] Setting current scene to '%s'", sceneName.c_str()); _logger(buf); }

    forceCleanupTransition();

    if (sceneCount > 0 && sceneStack[sceneCount - 1]) {
        _previousSceneName = _sceneNameStack[sceneCount - 1];
        _outgoingScene = sceneStack[sceneCount - 1];
        sceneStack[sceneCount - 1] = nullptr;
        _sceneNameStack[sceneCount - 1] = "";
        sceneCount--;
    } else {
        _previousSceneName = "";
        _outgoingScene = nullptr;
    }

    clearStack(); 

    Scene* newScene = createSceneByName(sceneName, configData); 
    if (!newScene) { 
        if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[SCENES] Failed to create scene '%s' for setCurrentScene.", sceneName.c_str()); _logger(buf); }
        cleanupOutgoingScene();
        return false; 
    }

    sceneStack[sceneCount] = newScene;
    _sceneNameStack[sceneCount] = sceneName;
    sceneCount++;
    newScene->onEnter();

    if (transition) {
        _activeTransition = transition;
        _activeTransition->begin(_outgoingScene, newScene);
        if (_logger) _logger("[SCENES] Scene transition started.");
    } else {
        cleanupOutgoingScene();
    }

    return true;
}

bool SceneManager::pushScene(const String& sceneName, void* configData, SceneTransition* transition) { 
     if (!inputManager) { 
        if (_logger) _logger("[SCENES] InputManager is null in pushScene.");
        return false; 
     }
     if (sceneCount >= MAX_SCENES) { 
        if (_logger) _logger("[SCENES] Scene stack full, cannot push.");
        return false; 
     }
     if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[SCENES] Pushing scene '%s'", sceneName.c_str()); _logger(buf); }

    forceCleanupTransition();

    if (sceneCount > 0 && sceneStack[sceneCount - 1]) {
        _previousSceneName = _sceneNameStack[sceneCount - 1];
        _outgoingScene = sceneStack[sceneCount - 1];
        sceneStack[sceneCount - 1] = nullptr;
        _sceneNameStack[sceneCount - 1] = "";
        sceneCount--;
    } else {
        _previousSceneName = "";
        _outgoingScene = nullptr;
    }

    Scene* newScene = createSceneByName(sceneName, configData); 
     if (!newScene) { 
        if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[SCENES] Failed to create scene '%s' for pushScene.", sceneName.c_str()); _logger(buf); }
        if (_outgoingScene) {
            sceneStack[sceneCount] = _outgoingScene;
            _sceneNameStack[sceneCount] = _previousSceneName;
            sceneCount++;
            _outgoingScene->onEnter();
            _outgoingScene = nullptr;
        }
        return false;
    }

    sceneStack[sceneCount] = newScene;
    _sceneNameStack[sceneCount] = sceneName;
    sceneCount++;
    newScene->onEnter();

    if (transition) {
        _activeTransition = transition;
        _activeTransition->begin(_outgoingScene, newScene);
        if (_logger) _logger("[SCENES] Scene transition started.");
    } else {
        if (_outgoingScene) {
            _outgoingScene->onExit();
            inputManager->unregisterAllListenersForScene(_outgoingScene);
            inputManager->clearDeferredActionsForScene(_outgoingScene);
            delete _outgoingScene;
            _outgoingScene = nullptr;
        }
    }

    return true;
}

bool SceneManager::popScene() {
     if (!inputManager) { 
        if (_logger) _logger("[SCENES] InputManager is null in popScene.");
        return false; 
     }
    if (sceneCount > 0) {
        Scene* removedScene = sceneStack[sceneCount - 1];
        String removedSceneName = _sceneNameStack[sceneCount - 1];
        if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[SCENES] Popping scene '%s'", removedSceneName.c_str()); _logger(buf); }

        if (removedScene) {
            _previousSceneName = removedSceneName;
            removedScene->onExit();
            inputManager->unregisterAllListenersForScene(removedScene);
            inputManager->clearDeferredActionsForScene(removedScene);
            delete removedScene;
        }
        sceneStack[sceneCount - 1] = nullptr;
        _sceneNameStack[sceneCount -1] = "";
        sceneCount--;

        if (sceneCount > 0 && sceneStack[sceneCount - 1]) {
             sceneStack[sceneCount - 1]->onEnter();
        }
        return true;
    } else { 
        if (_logger) _logger("[SCENES] Attempted to pop from an empty scene stack.");
        _previousSceneName = "";
        return false; 
    }
}

void SceneManager::update(unsigned long dt) {
    if (_activeTransition) {
        if (_activeTransition->shouldUpdateOutgoing() && _outgoingScene) {
            _outgoingScene->update(dt);
        }
        if (_activeTransition->shouldUpdateIncoming() && sceneCount > 0 && sceneStack[sceneCount - 1]) {
            sceneStack[sceneCount - 1]->update(dt);
        }
        if (_activeTransition->update(dt)) {
            if (_logger) _logger("[SCENES] Scene transition completed.");
            delete _activeTransition;
            _activeTransition = nullptr;
            cleanupOutgoingScene();
        }
    } else {
        if (sceneCount > 0 && sceneStack[sceneCount - 1]) {
            sceneStack[sceneCount - 1]->update(dt);
        }
    }
}

void SceneManager::draw(Renderer& rendererRef) { 
    if (_activeTransition) {
        _activeTransition->draw(rendererRef, _outgoingScene, sceneCount > 0 ? sceneStack[sceneCount - 1] : nullptr);
    } else {
        if (sceneCount > 0 && sceneStack[sceneCount - 1]) {
            sceneStack[sceneCount - 1]->draw(rendererRef);
        }
    }
}

bool SceneManager::shouldBlockInput() const {
    return _activeTransition && _activeTransition->shouldBlockInput();
}

void SceneManager::cleanupOutgoingScene() {
    if (_outgoingScene) {
        _outgoingScene->onExit();
        if (inputManager) {
            inputManager->unregisterAllListenersForScene(_outgoingScene);
            inputManager->clearDeferredActionsForScene(_outgoingScene);
        }
        if (_logger) {
            char buf[128];
            snprintf(buf, sizeof(buf), "[SCENES] Cleaning up outgoing scene '%s' (%p)", _previousSceneName.c_str(), (void*)_outgoingScene);
            _logger(buf);
        }
        delete _outgoingScene;
        _outgoingScene = nullptr;
    }
}

void SceneManager::forceCleanupTransition() {
    if (_activeTransition) {
        if (_logger) _logger("[SCENES] Force-cleaning up active transition.");
        delete _activeTransition;
        _activeTransition = nullptr;
    }
    cleanupOutgoingScene();
}

Scene* SceneManager::getCurrentScene() const {
    if (sceneCount > 0) {
        return sceneStack[sceneCount - 1];
    }
    return nullptr;
}

String SceneManager::getCurrentSceneName() const {
    if (sceneCount > 0) {
        return _sceneNameStack[sceneCount - 1];
    }
    return "";
}

SceneFactoryFunction SceneManager::getFactoryByName(const String& name) const {
    auto it = _sceneFactories.find(name.c_str());
    if (it != _sceneFactories.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<String> SceneManager::getRegisteredSceneNames() const {
    std::vector<String> names;
    for (const auto& pair : _sceneFactories) {
        names.push_back(pair.first.c_str());
    }
    return names;
}