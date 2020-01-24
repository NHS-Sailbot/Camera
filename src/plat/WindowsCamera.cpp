#include <Henry/Camera.hpp>

namespace Henry {
    static void defaultCameraUpdate(const Camera &c) {}

    Camera::Camera()
        : mWidth(0), mHeight(0), mDataSize(0), mId(0), mFlags(NONE), mOnUpdate(defaultCameraUpdate), mData(nullptr),
          mPixelData(nullptr) {}

    Camera::Camera(const unsigned int width, const unsigned int height)
        : mWidth(width), mHeight(height), mDataSize(0), mId(0), mFlags(NONE), mOnUpdate(defaultCameraUpdate), mData(nullptr),
          mPixelData(nullptr) {
        open(width, height);
    }

    Camera::~Camera() {
        close();
        delete[] mPixelData;
    }

    void Camera::open(const unsigned int width, const unsigned int height) {
    }

    void Camera::update() {
    }

    void Camera::close() {
    }
} // namespace Henry
