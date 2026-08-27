#include "SceneManager.h" 
#include "Scene.h"
#include "SceneTransition.h"
#include "InputManager.h"
#include "Renderer.h"
#include <Arduino.h>         
#include <vector>            
#include <string>
#include <cstring>

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
const char* SceneManager::getPendingSceneName() const { return _pendingNextSceneName; }
void* SceneManager::getPendingConfigData() const { return _pendingConfigData; }
bool SceneManager::getPendingReplaceStack() const { return _pendingReplaceStack; }
const char* SceneManager::getPreviousSceneName() const { return _previousSceneName; }
// --- END NEW GETTER IMPLEMENTATIONS ---

void SceneManager::processSceneChanges() {
    if (!_pendingSceneChange) {
        return;
    }

    if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[SCENES] Processing scene change request. Target: %s, Replace: %s", _pendingNextSceneName, _pendingReplaceStack ? "true" : "false"); _logger(buf); }

    char nameToSet[MAX_SCENE_NAME_LEN];
    strncpy(nameToSet, _pendingNextSceneName, sizeof(nameToSet) - 1);
    nameToSet[sizeof(nameToSet) - 1] = '\0';

    bool replace = _pendingReplaceStack;
    void* configPtr = _pendingConfigData;
    SceneTransition* transition = _pendingTransition;

    if (nameToSet[0] != '\0' && strcmp(nameToSet, "UNKNOWN") != 0) {
        bool success = false;
        if (replace) {
            success = setCurrentScene(nameToSet, configPtr, transition);
        } else {
            success = pushScene(nameToSet, configPtr, transition);
        }
        
        if (success) {
            _pendingTransition = nullptr; // Successfully handed off to activeTransition, clear it so clearPendingSceneChange doesn't delete it
        }
    } else {
        if (_logger) _logger("[SCENES] Scene change NOT processed. Target name was invalid.");
    }

    clearPendingSceneChange();
}

void SceneManager::setInputManager(InputManager* manager) {
    inputManager = manager;
}


bool SceneManager::registerScene(const char* name, SceneFactoryFunction factory) {
    if (!factory) {
        if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[SCENES] Tried to register a null factory for Scene '%s'", name ? name : "NULL"); _logger(buf); }
        return false;
    }
    if (!name || name[0] == '\0') {
        if (_logger) _logger("[SCENES] Scene name cannot be empty for registration.");
        return false;
    }

    auto it = std::find_if(_sceneFactories.begin(), _sceneFactories.end(), [&](const auto& pair) {
        return strcmp(pair.first, name) == 0;
    });

    if (it != _sceneFactories.end()) {
        if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[SCENES] Overwriting factory registration for Scene '%s'.", name); _logger(buf); }
        it->second = factory;
    } else {
        _sceneFactories.push_back({name, factory});
    }

    if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[SCENES] Registered Scene '%s' with its factory.", name); _logger(buf); }
    return true;
}

void SceneManager::requestSetCurrentScene(const char* sceneName, void* configData, SceneTransition* transition) {
    if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[SCENES] Requesting to SET current scene to '%s'", sceneName ? sceneName : "NULL"); _logger(buf); }
    
    if (_pendingTransition && _pendingTransition != transition) {
        if (_pendingTransition->autoDelete) {
            delete _pendingTransition;
        }
    }
    strncpy(_pendingNextSceneName, sceneName ? sceneName : "", sizeof(_pendingNextSceneName) - 1);
    _pendingNextSceneName[sizeof(_pendingNextSceneName) - 1] = '\0';
    _pendingConfigData = configData;
    _pendingTransition = transition;
    _pendingReplaceStack = true;
    _pendingSceneChange = true;
}

void SceneManager::requestPushScene(const char* sceneName, void* configData, SceneTransition* transition) {
    if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[SCENES] Requesting to PUSH scene '%s'", sceneName ? sceneName : "NULL"); _logger(buf); }

    if (_pendingTransition && _pendingTransition != transition) {
        if (_pendingTransition->autoDelete) {
            delete _pendingTransition;
        }
    }
    strncpy(_pendingNextSceneName, sceneName ? sceneName : "", sizeof(_pendingNextSceneName) - 1);
    _pendingNextSceneName[sizeof(_pendingNextSceneName) - 1] = '\0';
    _pendingConfigData = configData;
    _pendingTransition = transition;
    _pendingReplaceStack = false;
    _pendingSceneChange = true;
}

void SceneManager::clearPendingSceneChange() {
    _pendingSceneChange = false;
    _pendingNextSceneName[0] = '\0';
    _pendingConfigData = nullptr;
    if (_pendingTransition) {
        if (_pendingTransition->autoDelete) {
            delete _pendingTransition;
        }
        _pendingTransition = nullptr;
    }
}

void SceneManager::clearStack() {
    if (_logger) _logger("[SCENES] Clearing scene stack.");
    for (int i = sceneCount - 1; i >= 0; --i) {
        if (sceneStack[i]) {
            if (inputManager) {
                inputManager->unregisterAllListenersForScene(sceneStack[i]);
                inputManager->clearDeferredActionsForScene(sceneStack[i]);
            }
            if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[SCENES] Deleting scene '%s' (%p) from stack index %d", _sceneNameStack[i], sceneStack[i], i); _logger(buf); }
            delete sceneStack[i];
            sceneStack[i] = nullptr;
            _sceneNameStack[i][0] = '\0';
        }
    }
    sceneCount = 0;
}

Scene* SceneManager::createSceneByName(const char* sceneName, void* configData) {
    if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[SCENES] Attempting to create scene '%s' using factory.", sceneName ? sceneName : "NULL"); _logger(buf); }
    
    if (!sceneName) return nullptr;

    auto it = std::find_if(_sceneFactories.begin(), _sceneFactories.end(), [&](const auto& pair) {
        return strcmp(pair.first, sceneName) == 0;
    });

    if (it == _sceneFactories.end()) {
        if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[SCENES] No factory registered for Scene '%s'!", sceneName); _logger(buf); }
        return nullptr;
    }

    SceneFactoryFunction& factory = it->second;
    Scene* newScene = factory(configData); 

    if (newScene == nullptr) {
        if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[SCENES] Factory for Scene '%s' returned null!", sceneName); _logger(buf); }
        return nullptr;
    }

    if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[SCENES] Factory created scene '%s' at %p, calling generic init...", sceneName, (void*)newScene); _logger(buf); }
    // newScene->init(); // init() is now called by the factory function in Main.cpp

    return newScene;
}

Scene* SceneManager::setupNewScene(const char* sceneName, void* configData) {
    Scene* newScene = createSceneByName(sceneName, configData);
    if (!newScene) return nullptr;
    sceneStack[sceneCount] = newScene;
    strncpy(_sceneNameStack[sceneCount], sceneName ? sceneName : "", sizeof(_sceneNameStack[sceneCount]) - 1);
    _sceneNameStack[sceneCount][sizeof(_sceneNameStack[sceneCount]) - 1] = '\0';
    sceneCount++;
    newScene->onEnter();
    return newScene;
}


bool SceneManager::setCurrentScene(const char* sceneName, void* configData, SceneTransition* transition) { 
    if (!inputManager) { 
        if (_logger) _logger("[SCENES] InputManager is null in setCurrentScene.");
        return false; 
    }
    if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[SCENES] Setting current scene to '%s'", sceneName ? sceneName : "NULL"); _logger(buf); }

    forceCleanupTransition();

    if (sceneCount > 0 && sceneStack[sceneCount - 1]) {
        strncpy(_previousSceneName, _sceneNameStack[sceneCount - 1], sizeof(_previousSceneName) - 1);
        _previousSceneName[sizeof(_previousSceneName) - 1] = '\0';
        _outgoingScene = sceneStack[sceneCount - 1];
        sceneStack[sceneCount - 1] = nullptr;
        _sceneNameStack[sceneCount - 1][0] = '\0';
        sceneCount--;
    } else {
        _previousSceneName[0] = '\0';
        _outgoingScene = nullptr;
    }

    clearStack(); 

    Scene* newScene = setupNewScene(sceneName, configData);
    if (!newScene) { 
        if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[SCENES] Failed to create scene '%s' for setCurrentScene.", sceneName ? sceneName : "NULL"); _logger(buf); }
        cleanupOutgoingScene();
        return false; 
    }

    if (transition) {
        _activeTransition = transition;
        _activeTransition->begin(_outgoingScene, newScene);
        if (_logger) _logger("[SCENES] Scene transition started.");
    } else {
        cleanupOutgoingScene();
    }

    return true;
}

bool SceneManager::pushScene(const char* sceneName, void* configData, SceneTransition* transition) { 
     if (!inputManager) { 
        if (_logger) _logger("[SCENES] InputManager is null in pushScene.");
        return false; 
     }
     if (sceneCount >= MAX_SCENES) { 
        if (_logger) _logger("[SCENES] Scene stack full, cannot push.");
        return false; 
     }
     if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[SCENES] Pushing scene '%s'", sceneName ? sceneName : "NULL"); _logger(buf); }

    forceCleanupTransition();

    if (sceneCount > 0 && sceneStack[sceneCount - 1]) {
        strncpy(_previousSceneName, _sceneNameStack[sceneCount - 1], sizeof(_previousSceneName) - 1);
        _previousSceneName[sizeof(_previousSceneName) - 1] = '\0';
        _outgoingScene = sceneStack[sceneCount - 1];
        // Do NOT remove the outgoing scene from the stack or decrement sceneCount!
    } else {
        _previousSceneName[0] = '\0';
        _outgoingScene = nullptr;
    }

    Scene* newScene = setupNewScene(sceneName, configData);
     if (!newScene) { 
        if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[SCENES] Failed to create scene '%s' for pushScene.", sceneName ? sceneName : "NULL"); _logger(buf); }
        // FIX: _outgoingScene was never removed from the stack, so do not re-add it!
        // Just park the pointer safely.
        _outgoingScene = nullptr;
        return false;
    }

    if (transition) {
        _activeTransition = transition;
        _activeTransition->begin(_outgoingScene, newScene);
        if (_logger) _logger("[SCENES] Scene transition started.");
    } else {
        if (_outgoingScene) {
            _outgoingScene->onExit();
            // Do NOT delete the outgoing scene, it remains parked in the stack
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
    forceCleanupTransition(); // Stop any drawing pointers referring to about-to-be-deleted memory

    if (sceneCount > 0) {
        Scene* removedScene = sceneStack[sceneCount - 1];
        char removedSceneName[MAX_SCENE_NAME_LEN];
        strncpy(removedSceneName, _sceneNameStack[sceneCount - 1], sizeof(removedSceneName) - 1);
        removedSceneName[sizeof(removedSceneName) - 1] = '\0';
        if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[SCENES] Popping scene '%s'", removedSceneName); _logger(buf); }

        if (removedScene) {
            strncpy(_previousSceneName, removedSceneName, sizeof(_previousSceneName) - 1);
            _previousSceneName[sizeof(_previousSceneName) - 1] = '\0';
            removedScene->onExit();
            inputManager->unregisterAllListenersForScene(removedScene);
            inputManager->clearDeferredActionsForScene(removedScene);
            delete removedScene;
        }
        sceneStack[sceneCount - 1] = nullptr;
        _sceneNameStack[sceneCount - 1][0] = '\0';
        sceneCount--;

        if (sceneCount > 0 && sceneStack[sceneCount - 1]) {
             sceneStack[sceneCount - 1]->onEnter();
        }
        return true;
    } else { 
        if (_logger) _logger("[SCENES] Attempted to pop from an empty scene stack.");
        _previousSceneName[0] = '\0';
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
            if (_activeTransition->autoDelete) {
                delete _activeTransition;
            }
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

        // FIX: If pushScene was used, the scene remains parked on the stack and must NOT be deleted.
        bool keepAlive = false;
        for (int i = 0; i < sceneCount; i++) {
            if (sceneStack[i] == _outgoingScene) {
                keepAlive = true;
                break;
            }
        }

        if (!keepAlive) {
            if (inputManager) {
                inputManager->unregisterAllListenersForScene(_outgoingScene);
                inputManager->clearDeferredActionsForScene(_outgoingScene);
            }
            if (_logger) {
                char buf[128];
                snprintf(buf, sizeof(buf), "[SCENES] Cleaning up outgoing scene '%s' (%p)", _previousSceneName, (void*)_outgoingScene);
                _logger(buf);
            }
            delete _outgoingScene;
        }
        _outgoingScene = nullptr;
    }
}

void SceneManager::forceCleanupTransition() {
    if (_activeTransition) {
        if (_logger) _logger("[SCENES] Force-cleaning up active transition.");
        if (_activeTransition->autoDelete) {
            delete _activeTransition;
        }
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

const char* SceneManager::getCurrentSceneName() const {
    if (sceneCount > 0) {
        return _sceneNameStack[sceneCount - 1];
    }
    return "";
}

SceneFactoryFunction SceneManager::getFactoryByName(const char* name) const {
    if (!name) return nullptr;
    auto it = std::find_if(_sceneFactories.begin(), _sceneFactories.end(), [&](const auto& pair) {
        return strcmp(pair.first, name) == 0;
    });
    if (it != _sceneFactories.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<const char*> SceneManager::getRegisteredSceneNames() const {
    std::vector<const char*> names;
    for (const auto& pair : _sceneFactories) {
        names.push_back(pair.first);
    }
    return names;
}