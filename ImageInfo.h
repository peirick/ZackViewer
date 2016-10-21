#pragma once
#include <wincodec.h>
#include <d2d1.h>


class ImageInfo {
public:
    ImageInfo();
    HRESULT GetDefaultMetadata(IWICBitmapDecoder* decoder);
    HRESULT GetGlobalMetadata(IWICBitmapDecoder* decoder);

    void Reset();

    // The number of loops for which the animation will be played
    unsigned int     getTotalLoopCount()   const { return m_totalLoopCount; }

    // Whether the Image has a loop
    bool             hasLoop()             const { return m_hasLoop; }
    unsigned int     getFrameCount()       const { return m_frameCount; }

    unsigned int     getImageWidth()       const { return m_imageWidth; }
    unsigned int     getImageHeight()      const { return m_imageHeight; }

    // Width of the displayed image in pixel calculated using pixel aspect ratio
    unsigned int     getImageWidthPixel()  const { return m_imageWidthPixel; }

    // Height of the displayed image in pixel calculated using pixel aspect ratio
    unsigned int     getImageHeightPixel() const { return m_imageHeightPixel; }

    D2D1_COLOR_F	 getBackgroundColor()  const { return m_backgroundColor; }
private:
    HRESULT GetBackgroundColor(IWICBitmapDecoder* decoder, IWICMetadataQueryReader *pMetadataQueryReader);
public:
    unsigned int     m_totalLoopCount;
    bool             m_hasLoop;
    unsigned int     m_frameCount;
    unsigned int     m_imageWidth;
    unsigned int     m_imageHeight;
    unsigned int     m_imageWidthPixel;
    unsigned int     m_imageHeightPixel;
    D2D1_COLOR_F	 m_backgroundColor;
};
