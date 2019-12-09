#pragma once

namespace camera {
    struct Pixel {
        unsigned char r, g, b;
    };

    struct Device {
        Pixel *pixels;
        unsigned int width, height;
        unsigned char *data, is_open;
    };

    void open(Device &device, const unsigned int width, const unsigned int height);
    static inline Device open(const unsigned int width, const unsigned int height) {
        Device device;
        open(device, width, height);
        return device;
    }
    void read(Device &device);
} // namespace camera
