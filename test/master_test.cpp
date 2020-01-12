#include <HENRY/camera.hpp>
#include <debug/debug.hpp>

int main() {
    HENRY::Camera cam(1280, 720);
    if (!cam.is_open) {
        debug::log::error("unable to open camera");
        return -1;
    }
    cam.update();
}
