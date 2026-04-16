#pragma once

class Scene;
class Renderer;

class SceneTransition {
public:
    virtual ~SceneTransition() = default;

    virtual void begin(Scene* outgoing, Scene* incoming) {}

    virtual bool update(unsigned long dt) = 0;

    virtual void draw(Renderer& renderer, Scene* outgoing, Scene* incoming) = 0;

    virtual bool shouldUpdateOutgoing() const { return false; }

    virtual bool shouldUpdateIncoming() const { return true; }

    virtual bool shouldBlockInput() const { return true; }
};
