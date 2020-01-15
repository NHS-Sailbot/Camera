#pragma once

namespace Henry {
    class Camera {
      private:
        enum Flags { NONE = 0, OPEN_STATUS = 1, VALID_STATUS = 2 };
        unsigned int mWidth, mHeight, mDataSize;
        unsigned short mID, mFlags;
        void (*mOnUpdate)(const Camera &);
        unsigned char *mData;

      public:
        unsigned char *mPixelData;

        Camera();
        Camera(const unsigned int aWidth, const unsigned int aHeight);
        ~Camera();

        void Open(const unsigned int aWidth, const unsigned int aHeight);
        void Update();
        void Close();
        inline void OnUpdate(void (*const aFunc)(const Camera &)) { mOnUpdate = aFunc; }
        inline bool IsOpen() const { return mFlags & OPEN_STATUS; };
        inline bool IsValid() const { return mFlags & VALID_STATUS; };
    };
} // namespace Henry
