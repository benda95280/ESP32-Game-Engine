#pragma once

#include <functional> // For std::function
#include "Renderer.h"

// Define the logger type directly to break circular dependency with EDGE.h
using EDGELogger = std::function<void(const char* message)>;

class Scene {
public:
    using DeferredAction = std::function<void()>;

    virtual ~Scene();

    // Optional initialization - passed from GameContext or similar
    virtual void init(void* context = nullptr);

    // Called once when the scene becomes active
    virtual void onEnter() {}

    // Called once when the scene is deactivated
    virtual void onExit() {}

    // Called every frame to update game logic
    virtual void update(unsigned long deltaTime) = 0;

    // Called every frame to draw the scene
    virtual void draw(Renderer& renderer); 

    virtual bool doesManageOwnDrawing() const { return managesOwnDrawing; }
    
    virtual class DialogBox* getDialogBox() { return nullptr; }

    void setLogger(EDGELogger logger) { _logger = logger; }
    static void setMasterLogger(EDGELogger logger) { _masterLogger = logger; }

protected:
    bool managesOwnDrawing = false; // Set to true if scene handles its own firstPage/nextPage loop
    EDGELogger _logger;
    static EDGELogger _masterLogger;
};
