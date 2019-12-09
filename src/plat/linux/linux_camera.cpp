#include <camera/camera.hpp>
#include <debug/debug.hpp>

namespace camera {
    void open(Device &device, const unsigned int width, const unsigned int height) {
        device.width = width, device.height = height;
        debug::log::message("attempting to open camera...");
        device.is_open = false;
    }

    void read(Device &device) {
        //
    }
} // namespace camera
