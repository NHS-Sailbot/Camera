#include <HENRY/camera.hpp>

#define DEBUG_ENABLE_TIMING
#define DEBUG_ENABLE_LOGGING
#include <debug/debug.hpp>

#include <asm/types.h>
#include <linux/videodev2.h>

#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <errno.h>
#include <string.h>

namespace HENRY {
    static constexpr unsigned int buffer_count = 4;
    struct LinuxCameraDevice {
        v4l2_buffer bufferinfo[buffer_count];
        void *buffer_start[buffer_count];
        int buffer_index = 0, device_file_id = -1;
        unsigned long long current_time, last_tick_time;
    };
    static LinuxCameraDevice linux_devices[32];

    Camera::Camera(const unsigned int width, const unsigned int height) { open(width, height); }
    Camera::~Camera() { close(); }

    void Camera::open(const unsigned int width, const unsigned int height) {
        DEBUG_BEGIN_FUNC_PROFILE;

        this->width = width, this->height = height;

        DEBUG_BEGIN_PROFILE(select_index);
        unsigned int index = 0;
        for (; index < 32; ++index)
            if (linux_devices[index].device_file_id < 0) break;
        if (index > 31) {
            debug::log::error("Too many devices requested!");
            is_open = false;
            return;
        }
        DEBUG_END_PROFILE(select_index);

        auto &linux_device = linux_devices[index];
        const char *const filepath = "/dev/video0";
        debug::log::message("Attempting to open camera at %s...", filepath);

        DEBUG_BEGIN_PROFILE(open_file);
        linux_device.device_file_id = ::open(filepath, O_RDWR);
        if (!linux_device.device_file_id) {
            debug::log::error("unable to open video source");
            is_open = false;
            return;
        }
        DEBUG_END_PROFILE(open_file);
        // GET CAPABILITIES
        DEBUG_BEGIN_PROFILE(get_capabilities);
        v4l2_capability capability;
        if (ioctl(linux_device.device_file_id, VIDIOC_QUERYCAP, &capability)) {
            debug::log::error("unable to query video capability");
            ::close(linux_device.device_file_id);
            is_open = false;
            return;
        }
        if (!(capability.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
            debug::log::error("device does not handle single-planar video capture");
            ::close(linux_device.device_file_id);
            is_open = false;
            return;
        }
        DEBUG_END_PROFILE(get_capabilities);
        // SET FORMAT
        DEBUG_BEGIN_PROFILE(set_format);
        v4l2_format format;
        format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
        format.fmt.pix.width = width;
        format.fmt.pix.height = height;
        if (ioctl(linux_device.device_file_id, VIDIOC_S_FMT, &format) < 0) {
            debug::log::error("unable to set video capture format");
            ::close(linux_device.device_file_id);
            is_open = false;
            return;
        }
        DEBUG_END_PROFILE(set_format);
        // SET FRAMERATE
        DEBUG_BEGIN_PROFILE(set_framerate);
        v4l2_streamparm streamparam;
        streamparam.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        streamparam.parm.capture.timeperframe.numerator = 1;
        streamparam.parm.capture.timeperframe.denominator = 30;
        if (ioctl(linux_device.device_file_id, VIDIOC_S_PARM, &streamparam) == -1 ||
            ioctl(linux_device.device_file_id, VIDIOC_G_PARM, &streamparam) == -1) {
            debug::log::error("unable to set framerate");
            is_open = false;
            return;
        }
        DEBUG_END_PROFILE(set_framerate);
        // PREPARE FOR BUFFER HANDLING
        DEBUG_BEGIN_PROFILE(prepare_buffers);
        v4l2_requestbuffers bufrequest;
        bufrequest.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        bufrequest.memory = V4L2_MEMORY_MMAP;
        bufrequest.count = 4;
        if (ioctl(linux_device.device_file_id, VIDIOC_REQBUFS, &bufrequest) < 0) {
            debug::log::error("unable to request video buffers");
            ::close(linux_device.device_file_id);
            is_open = false;
            return;
        }
        for (unsigned char i = 0; i < buffer_count; ++i) {
            linux_device.bufferinfo[i].type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            linux_device.bufferinfo[i].memory = V4L2_MEMORY_MMAP;
            linux_device.bufferinfo[i].index = i;
            if (ioctl(linux_device.device_file_id, VIDIOC_QUERYBUF, &linux_device.bufferinfo[i]) < 0) {
                debug::log::error("unable to query video buffers");
                ::close(linux_device.device_file_id);
                is_open = false;
                return;
            }
            // MAP MEMORY
            linux_device.buffer_start[i] = mmap(NULL, linux_device.bufferinfo[i].length, PROT_READ | PROT_WRITE, MAP_SHARED,
                                                linux_device.device_file_id, linux_device.bufferinfo[i].m.offset);
            if (linux_device.buffer_start[i] == MAP_FAILED) {
                debug::log::error("mmap: %s", strerror(errno));
                is_open = false;
                return;
            }
            memset(linux_device.buffer_start[i], 0, linux_device.bufferinfo[i].length);
        }
        DEBUG_END_PROFILE(prepare_buffers);
        // Activate streaming
        DEBUG_BEGIN_PROFILE(activate_streaming);
        int type = linux_device.bufferinfo[0].type;
        if (ioctl(linux_device.device_file_id, VIDIOC_STREAMON, &type) < 0) {
            debug::log::error("VIDIOC_STREAMON: %s", strerror(errno));
            is_open = false;
            return;
        }
        DEBUG_END_PROFILE(activate_streaming);

        // Put each buffer into the queue
        DEBUG_BEGIN_PROFILE(put_buffers_in_queue);
        for (unsigned char i = 1; i < buffer_count; ++i) {
            if (ioctl(linux_device.device_file_id, VIDIOC_QBUF, &linux_device.bufferinfo[i]) < 0) {
                debug::log::error("VIDIOC_QBUF: %s", strerror(errno));
                is_open = false;
                return;
            }
        }
        DEBUG_END_PROFILE(put_buffers_in_queue);

        is_open = true;
        id = index;
        linux_device.current_time = debug::timer::micros::now(), linux_device.last_tick_time = linux_device.current_time;
        debug::log::success("Opened camera '%s' at %dx%d", filepath, width, height);
    }

    void Camera::update() {
        auto &linux_device = linux_devices[id];
        linux_device.current_time = debug::timer::micros::now();
        const auto elapsed = linux_device.current_time - linux_device.last_tick_time;

        if (elapsed > 33333) {
            DEBUG_BEGIN_FUNC_PROFILE;

            auto &linux_device = linux_devices[id];
            if (ioctl(linux_device.device_file_id, VIDIOC_QBUF, &linux_devices[id].bufferinfo[linux_device.buffer_index]) < 0) {
                debug::log::error("VIDIOC_QBUF2: %s", strerror(errno));
                return;
            }

            ++linux_device.buffer_index;
            if (linux_device.buffer_index == buffer_count) linux_device.buffer_index = 0;

            // The buffer's waiting in the outgoing queue.
            if (ioctl(linux_device.device_file_id, VIDIOC_DQBUF, &linux_device.bufferinfo[linux_device.buffer_index]) < 0) {
                debug::log::error("VIDIOC_DQBUF: %s", strerror(errno));
                return;
            }

            data = reinterpret_cast<unsigned char *>(linux_device.buffer_start[linux_device.buffer_index]);

            if (elapsed > 34000)
                linux_device.last_tick_time = debug::timer::micros::now();
            else
                linux_device.last_tick_time += 33333;
        }
    }

    void Camera::close() {
        DEBUG_BEGIN_FUNC_PROFILE;

        auto &linux_device = linux_devices[id];
        // Deactivate streaming
        int type = linux_device.bufferinfo[0].type;
        if (ioctl(linux_device.device_file_id, VIDIOC_STREAMOFF, &type) < 0) {
            debug::log::error("VIDIOC_STREAMOFF: %s", strerror(errno));
            return;
        }
        ::close(linux_device.device_file_id);
        linux_device.device_file_id = -1;
    }
} // namespace HENRY
