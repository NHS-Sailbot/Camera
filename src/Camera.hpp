#pragma once

namespace Henry {
    class Camera {
      private:
        enum Flags { NONE, OPEN_STATUS = 1, VALID_STATUS = 2 };
        unsigned int mWidth, mHeight, mDataSize;
        unsigned short mID, mFlags;
        void (*mOnUpdate)(const Camera &);
        unsigned char *mData, *const mPixelData;

      public:
        Camera(const unsigned int width, const unsigned int height);
        ~Camera();

        void Open();
        void Update();
        void Close();
        inline bool IsOpen() const { return mFlags & OPEN_STATUS; };
        inline bool IsValid() const { return mFlags & VALID_STATUS; };
    };
} // namespace Henry
