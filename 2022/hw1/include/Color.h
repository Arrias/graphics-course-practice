#pragma once

#include <cstdint>

struct Color {
    std::uint8_t data[4]{};

    explicit Color(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0);
};
