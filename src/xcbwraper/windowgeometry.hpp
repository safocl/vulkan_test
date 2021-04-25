#pragma once

#include <cstdint>

#include "point.hpp"

namespace xcbwraper {
class WindowGeometry final {
public:
    struct CreateInfo final {
        Point    leftTopPoint;
        uint16_t width;
        uint16_t height;
        uint16_t borderWidth;
    };

    struct Info final {
        Point    leftTopPoint;
        Point    rightTopPoint{};
        Point    leftBotPoint{};
        Point    rightBotPoint{};
        uint16_t width;
        uint16_t height;
        uint16_t borderWidth;
    };

    explicit WindowGeometry( WindowGeometry::CreateInfo wgci );
    Info getInfo() const;

    void reinit( CreateInfo wgci );

private:
    void compute();

private:
    Info wgInfo;
};
}   // namespace xcbwraper
