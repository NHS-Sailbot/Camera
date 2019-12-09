#include <camera/camera.hpp>
#include <debug/debug.hpp>

int main() {
    auto cam = camera::open(1280, 720);
    if (!cam.is_open) {
        debug::log::error("unable to open camera");
        return -1;
    }
    camera::read(cam);

    return 0;
}
