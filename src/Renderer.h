#pragma once
#include <stdint.h>

class Renderer {
public:
    virtual ~Renderer() = default;

    virtual void drawText(int x, int y, const char* str) = 0;
    virtual void drawTextSafe(int x, int y, const char* format, ...) = 0;
    virtual void drawCircle(int x, int y, int radius) = 0;
    virtual void drawFilledCircle(int x, int y, int radius) = 0;
    virtual void drawRectangle(int x, int y, int width, int height) = 0;
    virtual void drawFilledRectangle(int x, int y, int width, int height) = 0;
    virtual void drawLine(int x1, int y1, int x2, int y2) = 0;

    virtual void setContrast(uint8_t level) = 0;
    virtual void setFont(const uint8_t* font) = 0;

    virtual int getXOffset() const = 0;
    virtual int getYOffset() const = 0;
    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;

    // Returns a void* to the underlying native display object
    virtual void* getNativeDisplay() = 0; 
};
