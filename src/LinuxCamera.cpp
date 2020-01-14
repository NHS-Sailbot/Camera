#include "Camera.hpp"

#define DEBUG_ENABLE_TIMING
#define DEBUG_ENABLE_LOGGING
#include <Debug/Debug.hpp>

#include <asm/types.h>
#include <linux/videodev2.h>

#include <jpeglib.h>
#include <stdio.h>

#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <errno.h>
#include <string.h>

namespace Henry {
    static constexpr unsigned int BUFFER_COUNT = 4;
    struct LinuxCameraDevice {
        v4l2_buffer mBufferInfo[BUFFER_COUNT];
        void *mBufferStart[BUFFER_COUNT];
        int mBufferIndex = 0, mDeviceFileID = -1;
        unsigned long long mCurrentTime, mLastTickTime;
    };
    static LinuxCameraDevice sLinuxDevices[32];
    struct jpeg_decompress_struct sJpegInfo;
    struct jpeg_error_mgr sJpegError;
    static void defaultCameraUpdate(const Camera &c) {}

    Camera::Camera(const unsigned int width, const unsigned int height)
        : mWidth(width), mHeight(height), mDataSize(0), mID(0), mFlags(NONE), mOnUpdate(defaultCameraUpdate), mData(nullptr),
          mPixelData(new unsigned char[width * height * 3]) {
        Open();
    }

    Camera::~Camera() {
        Close();
        delete[] mPixelData;
    }

    void Camera::Open() {
        DEBUG_BEGIN_FUNC_PROFILE;

        DEBUG_BEGIN_PROFILE(dSelectIndex);
        unsigned int tIndex = 0;
        for (; tIndex < 32; ++tIndex)
            if (sLinuxDevices[tIndex].mDeviceFileID < 0) break;
        if (tIndex > 31) {
            Debug::Log::error("Too many devices requested!");
            mFlags &= !OPEN_STATUS;
            return;
        }
        DEBUG_END_PROFILE(dSelectIndex);

        auto &tLinuxDevice = sLinuxDevices[tIndex];
        const char *const tFilepath = "/dev/video0";
        Debug::Log::message("Attempting to open camera at %s...", tFilepath);

        DEBUG_BEGIN_PROFILE(dOpenDeviceFile);
        tLinuxDevice.mDeviceFileID = ::open(tFilepath, O_RDWR);
        if (!tLinuxDevice.mDeviceFileID) {
            Debug::Log::error("unable to open video source");
            mFlags &= !OPEN_STATUS;
            return;
        }
        DEBUG_END_PROFILE(dOpenDeviceFile);

        DEBUG_BEGIN_PROFILE(dVideo4LinuxGetCapabilities);
        v4l2_capability tCapability;
        if (ioctl(tLinuxDevice.mDeviceFileID, VIDIOC_QUERYCAP, &tCapability)) {
            Debug::Log::error("unable to query video capability");
            ::close(tLinuxDevice.mDeviceFileID);
            mFlags &= !Flags::OPEN_STATUS;
            return;
        }
        if (!(tCapability.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
            Debug::Log::error("device does not handle single-planar video capture");
            ::close(tLinuxDevice.mDeviceFileID);
            mFlags &= !OPEN_STATUS;
            return;
        }
        DEBUG_END_PROFILE(dVideo4LinuxGetCapabilities);

        DEBUG_BEGIN_PROFILE(dVideo4LinuxSetFormat);
        v4l2_format tFormat;
        tFormat.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        tFormat.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
        tFormat.fmt.pix.width = mWidth;
        tFormat.fmt.pix.height = mHeight;
        if (ioctl(tLinuxDevice.mDeviceFileID, VIDIOC_S_FMT, &tFormat) < 0) {
            Debug::Log::error("unable to set video capture format");
            ::close(tLinuxDevice.mDeviceFileID);
            mFlags &= !OPEN_STATUS;
            return;
        }
        DEBUG_END_PROFILE(dVideo4LinuxSetFormat);

        DEBUG_BEGIN_PROFILE(dVideo4LinuxSetFramerate);
        v4l2_streamparm tStreamParam;
        tStreamParam.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        tStreamParam.parm.capture.timeperframe.numerator = 1;
        tStreamParam.parm.capture.timeperframe.denominator = 30;
        if (ioctl(tLinuxDevice.mDeviceFileID, VIDIOC_S_PARM, &tStreamParam) == -1 ||
            ioctl(tLinuxDevice.mDeviceFileID, VIDIOC_G_PARM, &tStreamParam) == -1) {
            Debug::Log::error("unable to set framerate");
            mFlags &= !OPEN_STATUS;
            return;
        }
        DEBUG_END_PROFILE(dVideo4LinuxSetFramerate);

        DEBUG_BEGIN_PROFILE(dVideo4LinuxPrepareBuffers);
        v4l2_requestbuffers tBufRequest;
        tBufRequest.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        tBufRequest.memory = V4L2_MEMORY_MMAP;
        tBufRequest.count = 4;
        if (ioctl(tLinuxDevice.mDeviceFileID, VIDIOC_REQBUFS, &tBufRequest) < 0) {
            Debug::Log::error("unable to request video buffers");
            ::close(tLinuxDevice.mDeviceFileID);
            mFlags &= !OPEN_STATUS;
            return;
        }
        for (unsigned char i = 0; i < BUFFER_COUNT; ++i) {
            tLinuxDevice.mBufferInfo[i].type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            tLinuxDevice.mBufferInfo[i].memory = V4L2_MEMORY_MMAP;
            tLinuxDevice.mBufferInfo[i].index = i;
            if (ioctl(tLinuxDevice.mDeviceFileID, VIDIOC_QUERYBUF, &tLinuxDevice.mBufferInfo[i]) < 0) {
                Debug::Log::error("unable to query video buffers");
                ::close(tLinuxDevice.mDeviceFileID);
                mFlags &= !OPEN_STATUS;
                return;
            }
            tLinuxDevice.mBufferStart[i] = mmap(NULL, tLinuxDevice.mBufferInfo[i].length, PROT_READ | PROT_WRITE, MAP_SHARED,
                                                tLinuxDevice.mDeviceFileID, tLinuxDevice.mBufferInfo[i].m.offset);
            if (tLinuxDevice.mBufferStart[i] == MAP_FAILED) {
                Debug::Log::error("mmap: %s", strerror(errno));
                mFlags &= !OPEN_STATUS;
                return;
            }
            memset(tLinuxDevice.mBufferStart[i], 0, tLinuxDevice.mBufferInfo[i].length);
        }
        DEBUG_END_PROFILE(dVideo4LinuxPrepareBuffers);

        DEBUG_BEGIN_PROFILE(dVideo4LinuxActivateStreaming);
        int tType = tLinuxDevice.mBufferInfo[0].type;
        if (ioctl(tLinuxDevice.mDeviceFileID, VIDIOC_STREAMON, &tType) < 0) {
            Debug::Log::error("VIDIOC_STREAMON: %s", strerror(errno));
            mFlags &= !OPEN_STATUS;
            return;
        }
        DEBUG_END_PROFILE(dVideo4LinuxActivateStreaming);

        DEBUG_BEGIN_PROFILE(dVideo4LinuxPutBufferInQueue);
        for (unsigned char i = 1; i < BUFFER_COUNT; ++i) {
            if (ioctl(tLinuxDevice.mDeviceFileID, VIDIOC_QBUF, &tLinuxDevice.mBufferInfo[i]) < 0) {
                Debug::Log::error("VIDIOC_QBUF: %s", strerror(errno));
                mFlags &= !OPEN_STATUS;
                return;
            }
        }
        DEBUG_END_PROFILE(dVideo4LinuxPutBufferInQueue);

        DEBUG_BEGIN_PROFILE(dLibJpegInit);
        sJpegInfo.err = jpeg_std_error(&sJpegError);
        jpeg_create_decompress(&sJpegInfo);
        DEBUG_END_PROFILE(dLibJpegInit);

        mID = tIndex;
        mFlags |= OPEN_STATUS;
        Debug::Log::success("Opened camera '%s' at %dx%d", tFilepath, mWidth, mHeight);
        tLinuxDevice.mCurrentTime = Debug::Timer::Micros::now();
        tLinuxDevice.mLastTickTime = tLinuxDevice.mCurrentTime;
    }

    void Camera::Update() {
        auto &tLinuxDevice = sLinuxDevices[mID];
        tLinuxDevice.mCurrentTime = Debug::Timer::Micros::now();
        const auto tElapsed = tLinuxDevice.mCurrentTime - tLinuxDevice.mLastTickTime;

        if (tElapsed > 33333) {
            DEBUG_BEGIN_FUNC_PROFILE;

            if (ioctl(tLinuxDevice.mDeviceFileID, VIDIOC_QBUF, &sLinuxDevices[mID].mBufferInfo[tLinuxDevice.mBufferIndex]) <
                0) {
                Debug::Log::error("VIDIOC_QBUF2: %s", strerror(errno));
                return;
            }

            ++tLinuxDevice.mBufferIndex;
            if (tLinuxDevice.mBufferIndex == BUFFER_COUNT) tLinuxDevice.mBufferIndex = 0;

            if (ioctl(tLinuxDevice.mDeviceFileID, VIDIOC_DQBUF, &tLinuxDevice.mBufferInfo[tLinuxDevice.mBufferIndex]) < 0) {
                Debug::Log::error("VIDIOC_DQBUF: %s", strerror(errno));
                return;
            }

            mData = reinterpret_cast<unsigned char *>(tLinuxDevice.mBufferStart[tLinuxDevice.mBufferIndex]);
            mDataSize = tLinuxDevice.mBufferInfo[tLinuxDevice.mBufferIndex].length;

            if (!(mData == nullptr) && mDataSize > 0) {
                DEBUG_BEGIN_PROFILE(libjpeg_start);

                jpeg_mem_src(&sJpegInfo, mData, mDataSize);
                int rc = jpeg_read_header(&sJpegInfo, TRUE);
                jpeg_start_decompress(&sJpegInfo);

                while (sJpegInfo.output_scanline < sJpegInfo.output_height) {
                    unsigned char *temp_array[] = {mPixelData + (sJpegInfo.output_scanline) * mWidth * 3};
                    jpeg_read_scanlines(&sJpegInfo, temp_array, 1);
                }

                DEBUG_END_PROFILE(libjpeg_start);
                DEBUG_BEGIN_PROFILE(libjpeg_end);

                jpeg_finish_decompress(&sJpegInfo);

                DEBUG_END_PROFILE(libjpeg_end);
            }

            mOnUpdate(*this);

            if (tElapsed > 34000)
                tLinuxDevice.mLastTickTime = Debug::Timer::Micros::now();
            else
                tLinuxDevice.mLastTickTime += 33333;
        }
    }

    void Camera::Close() {
        DEBUG_BEGIN_FUNC_PROFILE;

        if (IsOpen()) {
            jpeg_destroy_decompress(&sJpegInfo);
            auto &tLinuxDevice = sLinuxDevices[mID];
            int type = tLinuxDevice.mBufferInfo[0].type;
            if (ioctl(tLinuxDevice.mDeviceFileID, VIDIOC_STREAMOFF, &type) < 0) {
                Debug::Log::error("VIDIOC_STREAMOFF: %s", strerror(errno));
                return;
            }
            ::close(tLinuxDevice.mDeviceFileID);
            tLinuxDevice.mDeviceFileID = -1;
        }
    }
} // namespace Henry
