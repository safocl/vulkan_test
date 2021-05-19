#pragma once

namespace core::renderer {

class Renderer {
public:
    virtual ~Renderer(){};
    virtual void draw() = 0;
};

}   // namespace core::renderer
