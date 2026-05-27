#include "EDGE.h"
#include "Scene.h" 
#include <Arduino.h>

// Constructor - Updated to accept a renderer reference and optional logger
EDGE::EDGE(Renderer& rendererRef, EDGELogger logger)
    : _logger(logger),
      sceneManager(), 
      renderer(rendererRef),
      inputManager(),
      previousMillis(0),
      deltaTime(0)
{
    if (_logger) {
        _logger("[CORE] EDGE Engine core constructed.");
    }
}

// Destructor
EDGE::~EDGE() {}

void EDGE::init() {
    // Propagate the logger to engine sub-components
    inputManager.setLogger(_logger);
    sceneManager.setLogger(_logger);
    
    inputManager.init();
    sceneManager.setInputManager(&inputManager);
    inputManager.setSceneManager(&sceneManager); 

    if (_logger) {
        _logger("[CORE] EDGE Engine initialized.");
    }
}

void EDGE::update(unsigned long forcedDeltaTime) {
    unsigned long currentMillis = millis();
    
    // Prevent massive delta time explosion on the very first frame after system boot
    if (previousMillis == 0) {
        previousMillis = currentMillis;
    }

    if (forcedDeltaTime > 0) {
        deltaTime = forcedDeltaTime;
        previousMillis = currentMillis; // Sync the tracker
    } else {
        deltaTime = currentMillis - previousMillis;
        previousMillis = currentMillis;
    }

    sceneManager.processSceneChanges();
    inputManager.update(deltaTime); 
    sceneManager.update(deltaTime); 
}

void EDGE::draw() {
    // Hardware-specific buffering is now handled entirely by the host application.
    sceneManager.draw(renderer);
}

Renderer& EDGE::getRenderer() { return renderer; }
InputManager& EDGE::getInputManager() { return inputManager; }
SceneManager& EDGE::getSceneManager() { return sceneManager; }
