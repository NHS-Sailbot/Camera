#pragma once

namespace HENRY {
    class Camera {
      public:
        struct Pixel {
            unsigned char r, g, b;
        };
        Pixel *pixels;
        unsigned int width, height, id;
        unsigned char *data, is_open;

        Camera(const unsigned int width, const unsigned int height);
        ~Camera();

        void open(const unsigned int width, const unsigned int height);
        void update();
        void close();
    };
} // namespace HENRY
