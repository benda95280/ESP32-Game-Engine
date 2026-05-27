#include "InputManager.h"
#include "SceneManager.h" 
#include <algorithm>      
#include <Arduino.h>      

extern void updateLastActivityTime();


void InputManager::init() {
    if (_queueMutex == nullptr) {
        _queueMutex = xSemaphoreCreateMutex();
    }
    listeners.clear();
    xSemaphoreTake(_queueMutex, portMAX_DELAY);
    _deferredActionsQueue.clear(); 
    xSemaphoreGive(_queueMutex);
}

void InputManager::update(unsigned long dt) {
    int actionsToProcessThisFrame = 0;
    xSemaphoreTake(_queueMutex, portMAX_DELAY);
    actionsToProcessThisFrame = _deferredActionsQueue.size();
    xSemaphoreGive(_queueMutex);

    actionsToProcessThisFrame = std::min(actionsToProcessThisFrame, 3); 
    
    for (int i = 0; i < actionsToProcessThisFrame; ++i) {
        DeferredActionEntry entry;
        xSemaphoreTake(_queueMutex, portMAX_DELAY);
        if (_deferredActionsQueue.empty()) {
            xSemaphoreGive(_queueMutex);
            break;
        }
        entry = _deferredActionsQueue.front();
        _deferredActionsQueue.pop_front();
        xSemaphoreGive(_queueMutex);
        
        if (entry.action) { 
            if (_logger) {
                char buf[128];
                snprintf(buf, sizeof(buf), "[INPUT] Executing deferred action for scene %p.", entry.ownerScene);
                _logger(buf);
            }
            entry.action(); 
        }
    }
}

void InputManager::setSceneManager(SceneManager* sm) {
    sceneManager = sm;
}

bool InputManager::registerButtonListener(EDGE_Button button, EDGE_Event eventType, Scene* scene, DeferredAction callback) {
    if (!scene || !callback) {
        if (_logger) _logger("[INPUT] Error: Invalid scene or callback provided for listener registration.");
        return false;
    }

    // Prevent duplicate registrations of the identical event/scene combo
    for (const auto& listener : listeners) {
        if (listener.button == button && listener.eventType == eventType && listener.scene == scene) {
            if (_logger) _logger("[INPUT] Warning: Listener already registered for this button/event/scene combo.");
            return false;
        }
    }

    listeners.push_back({button, eventType, scene, callback}); 
    if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[INPUT] InputManager: Registered listener for button %d, event %d, scene %p", (int)button, (int)eventType, scene); _logger(buf); }
    return true;
}
void InputManager::unregisterButtonListener(EDGE_Button button, EDGE_Event eventType, Scene* scene) {
    std::erase_if(listeners, [button, eventType, scene](const ListenerInfo& listener) {
        return listener.button == button && listener.eventType == eventType && listener.scene == scene;
    });
    if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[INPUT] InputManager: Unregistered listener for button %d, event %d, scene %p", (int)button, (int)eventType, scene); _logger(buf); }
}

void InputManager::unregisterAllListenersForScene(Scene* scene) {
    if (!scene) return;
    std::erase_if(listeners, [scene](const ListenerInfo& listener) {
        return listener.scene == scene;
    });
    if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[INPUT] InputManager: Unregistered all listeners for scene %p", scene); _logger(buf); }
}

void InputManager::processButtonEvent(EDGE_Button button, EDGE_Event eventType) {
    updateLastActivityTime(); 

    if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[INPUT] InputManager::processButtonEvent: Received button %d, event %d", (int)button, (int)eventType); _logger(buf); }

    if (!sceneManager) {
        if (_logger) _logger("[INPUT] InputManager Warning: SceneManager not set during event processing!");
        return;
    }

    if (sceneManager->shouldBlockInput()) {
        if (_logger) _logger("[INPUT] InputManager: Input blocked during scene transition.");
        return;
    }

    Scene* currentScene = sceneManager->getCurrentScene();
    // [FIX] Avoid String allocation to prevent heap fragmentation and UB in snprintf
    const char* currentSceneName = sceneManager->getCurrentSceneName();
    if (!currentScene) {
        if (_logger) _logger("[INPUT] InputManager Warning: No active scene to process event!");
        return; 
    }

    bool hasListeners = false;
    for (const auto& listener : listeners) {
        if (listener.scene == currentScene) {
            hasListeners = true;
            break;
        }
    }

    if (!hasListeners) {
        static uint32_t lastWarnTime = 0;
        uint32_t now = millis();
        if (_logger && (now - lastWarnTime > 1000)) {
            char buf[150];
            snprintf(buf, sizeof(buf), "[INPUT] No listeners for scene '%s', ignoring button events", currentSceneName);
            _logger(buf);
            lastWarnTime = now;
        }
        return;
    }

    bool eventDeferred = false;
    for (const auto& listener : listeners) {
        if (listener.button == button && listener.eventType == eventType && listener.scene == currentScene) {
            if (listener.callback) {
                deferAction(currentScene, listener.callback);
                eventDeferred = true;
                if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[INPUT] InputManager: Deferred direct callback for button %d, event %d on scene %p.", (int)button, (int)eventType, currentScene); _logger(buf); }
            }
        }
    }

    if (!eventDeferred) {
        if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[INPUT] InputManager: No direct callback found or deferred for button %d, event %d on non-queued scene %p.", (int)button, (int)eventType, currentScene); _logger(buf); }
    }
}

void InputManager::deferAction(Scene* ownerScene, DeferredAction action) {
    if (!ownerScene || !action) return;
    xSemaphoreTake(_queueMutex, portMAX_DELAY);
    _deferredActionsQueue.push_back({ownerScene, action});
    xSemaphoreGive(_queueMutex);
}

void InputManager::clearDeferredActionsForScene(Scene* scene) {
    if (!scene) return;
    xSemaphoreTake(_queueMutex, portMAX_DELAY);
    size_t initialSize = _deferredActionsQueue.size();
    std::erase_if(_deferredActionsQueue, [scene](const DeferredActionEntry& entry) {
        return entry.ownerScene == scene;
    });
    size_t removedCount = initialSize - _deferredActionsQueue.size();
    xSemaphoreGive(_queueMutex);
    if (removedCount > 0) {
        if (_logger) { char buf[128]; snprintf(buf, sizeof(buf), "[INPUT] InputManager: Cleared %u deferred actions for scene %p", (unsigned int)removedCount, scene); _logger(buf); }
    }
}
