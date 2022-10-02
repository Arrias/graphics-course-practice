#include "Color.h"

Color::Color(uint8_t r, uint8_t g, uint8_t b) {
    data[0] = r;
    data[1] = g;
    data[2] = b;
    data[3] = 0;
}
