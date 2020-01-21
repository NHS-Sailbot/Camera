#include "Camera.hpp"

#include <asm/types.h>
#include <linux/videodev2.h>

#include <stdio.h>
#include <jpeglib.h>

#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <errno.h>
#include <string.h>

#include <chrono>

namespace Henry {
	static constexpr unsigned int BUFFER_COUNT = 4, FRAMES_PER_SECOND = 30;
	static constexpr unsigned long long FRAME_INTERVAL = 1000000000 / FRAMES_PER_SECOND;

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

	using namespace std::chrono;
	static auto sGlobalStart = steady_clock::now();
	static inline unsigned long long now() { return duration_cast<nanoseconds>(steady_clock::now() - sGlobalStart).count(); }
	static inline void sleep(const unsigned long long ns) {
		const auto tStartTime = now();
		while (now() - tStartTime < ns) {}
	}

	Camera::Camera() :
	mWidth(0), mHeight(0), mDataSize(0), mId(0), mFlags(NONE), mOnUpdate(defaultCameraUpdate), mData(nullptr),
	mPixelData(nullptr) {}

	Camera::Camera(const unsigned int width, const unsigned int height) :
	mWidth(width), mHeight(height), mDataSize(0), mId(0), mFlags(NONE), mOnUpdate(defaultCameraUpdate), mData(nullptr),
	mPixelData(nullptr) {
		open(width, height);
	}

	Camera::~Camera() {
		close();
		delete[] mPixelData;
	}

	void Camera::open(const unsigned int width, const unsigned int height) {
		mWidth = width, mHeight = height;
		if (mPixelData) delete[] mPixelData;
		mPixelData = new unsigned char[mWidth * mHeight * 3];
		unsigned int tIndex = 0;
		for (; tIndex < 32; ++tIndex)
			if (sLinuxDevices[tIndex].mDeviceFileID < 0) break;
		if (tIndex > 31) {
			mFlags &= ~OPEN_STATUS;
			return;
		}

		auto &tLinuxDevice = sLinuxDevices[tIndex];
		const char *const tFilepath = "/dev/video0";

		tLinuxDevice.mDeviceFileID = ::open(tFilepath, O_RDWR);
		if (!tLinuxDevice.mDeviceFileID) {
			mFlags &= ~OPEN_STATUS;
			return;
		}

		v4l2_capability tCapability;
		if (ioctl(tLinuxDevice.mDeviceFileID, VIDIOC_QUERYCAP, &tCapability)) {
			::close(tLinuxDevice.mDeviceFileID);
			mFlags &= ~OPEN_STATUS;
			return;
		}
		if (!(tCapability.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
			::close(tLinuxDevice.mDeviceFileID);
			mFlags &= ~OPEN_STATUS;
			return;
		}

		v4l2_format tFormat;
		tFormat.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		tFormat.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
		tFormat.fmt.pix.width = mWidth;
		tFormat.fmt.pix.height = mHeight;
		if (ioctl(tLinuxDevice.mDeviceFileID, VIDIOC_S_FMT, &tFormat) < 0) {
			::close(tLinuxDevice.mDeviceFileID);
			mFlags &= ~OPEN_STATUS;
			return;
		}

		v4l2_streamparm tStreamParam;
		tStreamParam.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		tStreamParam.parm.capture.timeperframe.numerator = 1;
		tStreamParam.parm.capture.timeperframe.denominator = FRAMES_PER_SECOND;
		if (ioctl(tLinuxDevice.mDeviceFileID, VIDIOC_S_PARM, &tStreamParam) == -1 ||
		ioctl(tLinuxDevice.mDeviceFileID, VIDIOC_G_PARM, &tStreamParam) == -1) {
			mFlags &= ~OPEN_STATUS;
			return;
		}

		v4l2_requestbuffers tBufRequest;
		tBufRequest.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		tBufRequest.memory = V4L2_MEMORY_MMAP;
		tBufRequest.count = BUFFER_COUNT;
		if (ioctl(tLinuxDevice.mDeviceFileID, VIDIOC_REQBUFS, &tBufRequest) < 0) {
			::close(tLinuxDevice.mDeviceFileID);
			mFlags &= ~OPEN_STATUS;
			return;
		}
		for (unsigned char i = 0; i < BUFFER_COUNT; ++i) {
			tLinuxDevice.mBufferInfo[i].type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			tLinuxDevice.mBufferInfo[i].memory = V4L2_MEMORY_MMAP;
			tLinuxDevice.mBufferInfo[i].index = i;
			if (ioctl(tLinuxDevice.mDeviceFileID, VIDIOC_QUERYBUF, &tLinuxDevice.mBufferInfo[i]) < 0) {
				::close(tLinuxDevice.mDeviceFileID);
				mFlags &= ~OPEN_STATUS;
				return;
			}
			tLinuxDevice.mBufferStart[i] = mmap(NULL, tLinuxDevice.mBufferInfo[i].length, PROT_READ | PROT_WRITE, MAP_SHARED,
			tLinuxDevice.mDeviceFileID, tLinuxDevice.mBufferInfo[i].m.offset);
			if (tLinuxDevice.mBufferStart[i] == MAP_FAILED) {
				mFlags &= ~OPEN_STATUS;
				return;
			}
			memset(tLinuxDevice.mBufferStart[i], 0, tLinuxDevice.mBufferInfo[i].length);
		}

		int tType = tLinuxDevice.mBufferInfo[0].type;
		if (ioctl(tLinuxDevice.mDeviceFileID, VIDIOC_STREAMON, &tType) < 0) {
			mFlags &= ~OPEN_STATUS;
			return;
		}

		for (unsigned char i = 1; i < BUFFER_COUNT; ++i) {
			if (ioctl(tLinuxDevice.mDeviceFileID, VIDIOC_QBUF, &tLinuxDevice.mBufferInfo[i]) < 0) {
				mFlags &= ~OPEN_STATUS;
				return;
			}
		}

		sJpegInfo.err = jpeg_std_error(&sJpegError);
		jpeg_create_decompress(&sJpegInfo);

		mId = tIndex;
		mFlags |= OPEN_STATUS;
		tLinuxDevice.mCurrentTime = now();
		tLinuxDevice.mLastTickTime = tLinuxDevice.mCurrentTime;
	}

	void Camera::update() {
		auto &tLinuxDevice = sLinuxDevices[mId];
		tLinuxDevice.mCurrentTime = now();
		const auto tElapsed = tLinuxDevice.mCurrentTime - tLinuxDevice.mLastTickTime;

		if (tElapsed > FRAME_INTERVAL) {
			if (ioctl(tLinuxDevice.mDeviceFileID, VIDIOC_QBUF, &sLinuxDevices[mId].mBufferInfo[tLinuxDevice.mBufferIndex]) < 0) return;

			++tLinuxDevice.mBufferIndex;
			if (tLinuxDevice.mBufferIndex == BUFFER_COUNT) tLinuxDevice.mBufferIndex = 0;

			if (ioctl(tLinuxDevice.mDeviceFileID, VIDIOC_DQBUF, &tLinuxDevice.mBufferInfo[tLinuxDevice.mBufferIndex]) < 0) return;

			mData = reinterpret_cast<unsigned char *>(tLinuxDevice.mBufferStart[tLinuxDevice.mBufferIndex]);
			mDataSize = tLinuxDevice.mBufferInfo[tLinuxDevice.mBufferIndex].length;

			if (!(mData == nullptr) && mDataSize > 0) {
				jpeg_mem_src(&sJpegInfo, mData, mDataSize);
				int rc = jpeg_read_header(&sJpegInfo, TRUE);
				jpeg_start_decompress(&sJpegInfo);
				while (sJpegInfo.output_scanline < sJpegInfo.output_height) {
					unsigned char *temp_array[] = {mPixelData + (sJpegInfo.output_scanline) * mWidth * 3};
					jpeg_read_scanlines(&sJpegInfo, temp_array, 1);
				}
				jpeg_finish_decompress(&sJpegInfo);
			}

			mOnUpdate(*this);

			if (tElapsed > FRAME_INTERVAL + 100000)
				tLinuxDevice.mLastTickTime = now();
			else
				tLinuxDevice.mLastTickTime += FRAME_INTERVAL;
		}
	}

	void Camera::close() {
		if (isOpen()) {
			jpeg_destroy_decompress(&sJpegInfo);
			auto &tLinuxDevice = sLinuxDevices[mId];
			int type = tLinuxDevice.mBufferInfo[0].type;
			if (ioctl(tLinuxDevice.mDeviceFileID, VIDIOC_STREAMOFF, &type) < 0)
				return;
			::close(tLinuxDevice.mDeviceFileID);
			tLinuxDevice.mDeviceFileID = -1;
		}
	}
} // namespace Henry
