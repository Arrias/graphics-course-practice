#include "Color.h"

Color::Color(uint8_t r, uint8_t g, uint8_t b) {
    data[0] = r;
    data[1] = g;
    data[2] = b;
    data[3] = 0;
}

Color BlueColor() {
    return Color(0, 0, 255);
}

Color YellowColor() {
    return Color(255, 255, 0);
}
