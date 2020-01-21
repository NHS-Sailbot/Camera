#pragma once

namespace Henry {
	class Camera {
	  private:
		enum Flags {
			NONE = 0,
			OPEN_STATUS = 1,
			VALID_STATUS = 2
		};
		unsigned int mWidth, mHeight, mDataSize;
		unsigned short mId, mFlags;
		void (*mOnUpdate)(const Camera &);
		unsigned char *mData;

	  public:
		unsigned char *mPixelData;

		Camera();
		Camera(const unsigned int width, const unsigned int height);
		~Camera();

		void open(const unsigned int width, const unsigned int height);
		void update();
		void close();
		inline void onUpdate(void (*const f)(const Camera &)) { mOnUpdate = f; }
		inline bool isOpen() const { return mFlags & OPEN_STATUS; };
		inline bool isValid() const { return mFlags & VALID_STATUS; };
	};
} // namespace Henry
