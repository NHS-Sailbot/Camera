#pragma once

namespace HENRY {
    class Camera {
      public:
        struct Pixel {
            unsigned char r, g, b;
        };
        unsigned int width, height, id, data_size;
        void (*on_update)(const Camera &);
        unsigned char *data, *const pixels, is_open;

        Camera(const unsigned int width, const unsigned int height);
        ~Camera();

        void open();
        void update();
        void close();
    };
} // namespace HENRY
