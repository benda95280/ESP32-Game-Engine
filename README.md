# The ESP Device Game Engine (EDGE) - Rewrite

## About This Rewrite

This repository is a comprehensive rewrite of the original [ESP32-Game-Engine by Nicolas Bourré](https://github.com/nbourre/ESP32-Game-Engine). While building on its core concepts, this version introduces significant architectural changes to improve flexibility, modularity, and portability.

### Key Changes in this Rewrite

*   **Display-Agnostic Core (Pure Abstraction):** The engine is now completely decoupled from specific hardware or graphics libraries. It uses an abstract `Renderer` interface. The host application provides a concrete implementation of this interface (e.g., wrapping `U8G2`, `Adafruit_GFX`, or even a serial terminal).

*   **Core Architectural Refactoring (Dependency Injection):** The engine and its components no longer manage hardware initialization. All dependencies (Renderer, Logger) are injected at runtime, allowing the engine to run in diverse environments.

*   **Advanced Scene Management:** The scene manager has been completely overhauled.
    *   **Factory-Based Creation:** Scenes are now registered with a name and created via a factory function (`registerScene`).
    *   **Scene Stack:** The manager operates as a proper stack, with `pushScene` and `popScene` methods.
    *   **Lifecycle Hooks:** Scenes have `onEnter()` and `onExit()` methods, called automatically during transitions.

*   **Reworked Input System:** The `InputManager` uses generic enums (`EDGE_Button`, `EDGE_Event`) and a deferred action queue to process inputs without being tied to specific hardware buttons or external UI libraries.

*   **Integrated Logging:** A flexible, callback-based logging system (`EDGELogger`) is integrated throughout the engine core.

---

# Table of content <!-- omit in toc -->

- [The ESP Device Game Engine (EDGE)](#the-esp-device-game-engine-edge)
  - [Features](#features)
  - [Project Structure](#project-structure)
  - [Usage](#usage)
  - [License](#license)
  - [Credits](#credits)

---

## Features
- **Display-agnostic architecture** via the abstract `Renderer` interface.
- **Dependency-free core**: No mandatory external library dependencies within the engine itself.
- **Advanced scene management** with a scene stack, factories, and lifecycle hooks.
- **Abstracted input system** using deferred callbacks and generic enums.
- **Integrated callback-based logger** for flexible debugging.
- **Optimized for embedded systems** with a focus on memory efficiency and decoupling.

---

## Project Structure
```
EDGE/
├── src/
│   ├── EDGE.h / EDGE.cpp              # Core engine, ties components together
│   ├── Scene.h / Scene.cpp            # Base class for all scenes
│   ├── SceneManager.h / SceneManager.cpp  # Handles scene stack, factories, and transitions
│   ├── Renderer.h                     # Abstract graphics rendering interface
│   ├── InputManager.h / InputManager.cpp  # Abstracted button/event handling
├── README.md                          # Documentation
├── platformio.ini                     # PlatformIO project configuration
```

---

## Usage

To use EDGE, your application must provide a concrete implementation of the `Renderer` interface.

```cpp
class MyRenderer : public Renderer {
    // Implement drawText, drawCircle, getNativeDisplay, etc.
};

// ... in setup ...
MyRenderer renderer;
EDGE engine(renderer, [](const char* msg) { Serial.println(msg); });
engine.init();
```

---

## License
This project is open-source under the **MIT License**.

The original work by **Nicolas Bourré** is also licensed under the MIT License. This rewritten version is a derivative work and is therefore also provided under the same license terms.

---

## Credits
- Original engine concept and implementation by **Nicolas Bourré**.
- This version rewritten and refactored by **benda95280**.
