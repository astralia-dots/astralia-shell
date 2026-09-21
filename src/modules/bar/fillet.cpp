#include <algorithm>
#include <cmath>

#include "modules/bar/fillet.h"

std::vector<uint8_t> fillet_rgba(int size, bool circle_on_right) {
    std::vector<uint8_t> rgba(static_cast<size_t>(size) * size * 4, 255);
    float radius = static_cast<float>(size);
    float cx = circle_on_right ? radius : 0.0f;
    for (int py = 0; py < size; ++py) {
        for (int px = 0; px < size; ++px) {
            float dist = std::hypot(static_cast<float>(px) + 0.5f - cx, static_cast<float>(py) + 0.5f - radius);
            float alpha = std::clamp(dist - radius + 0.5f, 0.0f, 1.0f);
            rgba[(static_cast<size_t>(py) * size + px) * 4 + 3] = static_cast<uint8_t>(alpha * 255.0f + 0.5f);
        }
    }
    return rgba;
}
