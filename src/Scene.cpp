#include "Scene.h"
#include <Arduino.h> 

// Define the static logger member
EDGELogger Scene::_masterLogger = nullptr;

Scene::~Scene() {
}

void Scene::init(void* context) { 
    _logger = _masterLogger; // Default to master logger
}

void Scene::draw(Renderer& renderer) { 
    //Empty
}
