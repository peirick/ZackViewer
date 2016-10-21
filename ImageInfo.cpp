#include "ImageInfo.h"
#include "ComPtr.h"
#include "ImagingFactorySingleton.h"

ImageInfo::ImageInfo()
{
    Reset();
}

HRESULT ImageInfo::GetDefaultMetadata(IWICBitmapDecoder* decoder)
{
    // Default to transparent if failed to get the color
    m_backgroundColor = D2D1::ColorF(0, 0.f);
    m_hasLoop = false;

    // Get the frame count
    HRESULT hr = decoder->GetFrameCount(&m_frameCount);
    if (m_frameCount == 0)
    {
        return S_FALSE;
    }

    ComPtr<IWICBitmapFrameDecode> pWICBitmapFrameDecode;
    if (SUCCEEDED(hr))
    {
        hr = decoder->GetFrame(0, pWICBitmapFrameDecode.get_out_storage());
    }

    // Get global frame size
    if (SUCCEEDED(hr))
    {
        hr = pWICBitmapFrameDecode->GetSize(&m_imageWidth, &m_imageHeight);

        // Get pixel aspect ratio
        m_imageWidthPixel = m_imageWidth;
        m_imageHeightPixel = m_imageHeight;
    }
    return hr;
}

HRESULT ImageInfo::GetGlobalMetadata(IWICBitmapDecoder* decoder)
{
    PROPVARIANT propValue;
    PropVariantInit(&propValue);
    ComPtr<IWICMetadataQueryReader> pMetadataQueryReader;

    // Get the frame count
    HRESULT hr = decoder->GetFrameCount(&m_frameCount);
    if (SUCCEEDED(hr))
    {
        // Create a MetadataQueryReader from the decoder
        hr = decoder->GetMetadataQueryReader(pMetadataQueryReader.get_out_storage());
    }

    if (SUCCEEDED(hr))
    {
        // Get background color
        if (FAILED(GetBackgroundColor(decoder, pMetadataQueryReader.get())))
        {
            // Default to transparent if failed to get the color
            m_backgroundColor = D2D1::ColorF(0, 0.f);
        }
    }

    // Get global frame size
    if (SUCCEEDED(hr))
    {
        // Get width
        hr = pMetadataQueryReader->GetMetadataByName(
            L"/logscrdesc/Width",
            &propValue);
        if (SUCCEEDED(hr))
        {
            hr = (propValue.vt == VT_UI2 ? S_OK : E_FAIL);
            if (SUCCEEDED(hr))
            {
                m_imageWidth = propValue.uiVal;
            }
            PropVariantClear(&propValue);
        }
    }

    if (SUCCEEDED(hr))
    {
        // Get height
        hr = pMetadataQueryReader->GetMetadataByName(
            L"/logscrdesc/Height",
            &propValue);
        if (SUCCEEDED(hr))
        {
            hr = (propValue.vt == VT_UI2 ? S_OK : E_FAIL);
            if (SUCCEEDED(hr))
            {
                m_imageHeight = propValue.uiVal;
            }
            PropVariantClear(&propValue);
        }
    }

    if (SUCCEEDED(hr))
    {
        // Get pixel aspect ratio
        hr = pMetadataQueryReader->GetMetadataByName(
            L"/logscrdesc/PixelAspectRatio",
            &propValue);
        if (SUCCEEDED(hr))
        {
            hr = (propValue.vt == VT_UI1 ? S_OK : E_FAIL);
            if (SUCCEEDED(hr))
            {
                UINT uPixelAspRatio = propValue.bVal;

                if (uPixelAspRatio != 0)
                {
                    // Need to calculate the ratio. The value in uPixelAspRatio 
                    // allows specifying widest pixel 4:1 to the tallest pixel of 
                    // 1:4 in increments of 1/64th
                    float pixelAspRatio = (uPixelAspRatio + 15.f) / 64.f;

                    // Calculate the image width and height in pixel based on the
                    // pixel aspect ratio. Only shrink the image.
                    if (pixelAspRatio > 1.f)
                    {
                        m_imageWidthPixel = m_imageWidth;
                        m_imageHeightPixel = static_cast<unsigned int>(m_imageHeight / pixelAspRatio);
                    }
                    else
                    {
                        m_imageWidthPixel = static_cast<unsigned int>(m_imageWidth * pixelAspRatio);
                        m_imageHeightPixel = m_imageHeight;
                    }
                }
                else
                {
                    // The value is 0, so its ratio is 1
                    m_imageWidthPixel = m_imageWidth;
                    m_imageHeightPixel = m_imageHeight;
                }
            }
            PropVariantClear(&propValue);
        }
    }

    // Get looping information
    if (SUCCEEDED(hr))
    {
        // First check to see if the application block in the Application Extension
        // contains "NETSCAPE2.0" and "ANIMEXTS1.0", which indicates the gif animation
        // has looping information associated with it.
        // 
        // If we fail to get the looping information, loop the animation infinitely.
        if (SUCCEEDED(pMetadataQueryReader->GetMetadataByName(
            L"/appext/application",
            &propValue)) &&
            propValue.vt == (VT_UI1 | VT_VECTOR) &&
            propValue.caub.cElems == 11 &&  // Length of the application block
            (!memcmp(propValue.caub.pElems, "NETSCAPE2.0", propValue.caub.cElems) ||
                !memcmp(propValue.caub.pElems, "ANIMEXTS1.0", propValue.caub.cElems)))
        {
            PropVariantClear(&propValue);

            hr = pMetadataQueryReader->GetMetadataByName(L"/appext/data", &propValue);
            if (SUCCEEDED(hr))
            {
                //  The data is in the following format:
                //  byte 0: extsize (must be > 1)
                //  byte 1: loopType (1 == animated gif)
                //  byte 2: loop count (least significant byte)
                //  byte 3: loop count (most significant byte)
                //  byte 4: set to zero
                if (propValue.vt == (VT_UI1 | VT_VECTOR) &&
                    propValue.caub.cElems >= 4 &&
                    propValue.caub.pElems[0] > 0 &&
                    propValue.caub.pElems[1] == 1)
                {
                    m_totalLoopCount = MAKEWORD(propValue.caub.pElems[2],
                        propValue.caub.pElems[3]);

                    // If the total loop count is not zero, we then have a loop count
                    // If it is 0, then we repeat infinitely
                    if (m_totalLoopCount != 0)
                    {
                        m_hasLoop = true;
                    }
                }
            }
        }
    }

    PropVariantClear(&propValue);
    return hr;
}

void ImageInfo::Reset()
{
    m_totalLoopCount = 0;
    m_hasLoop = false;
    m_frameCount = 0;
    m_imageWidth = 0;
    m_imageHeight = 0;
    m_imageWidthPixel = 0;
    m_imageHeightPixel = 0;
    m_backgroundColor = D2D1::ColorF(0, 0.f);;
}

HRESULT ImageInfo::GetBackgroundColor(IWICBitmapDecoder* decoder, IWICMetadataQueryReader* pMetadataQueryReader)
{
    DWORD dwBGColor;
    BYTE backgroundIndex = 0;
    WICColor rgColors[256];
    UINT cColorsCopied = 0;
    PROPVARIANT propVariant;
    PropVariantInit(&propVariant);
    ComPtr<IWICPalette> pWicPalette;

    // If we have a global palette, get the palette and background color
    HRESULT hr = pMetadataQueryReader->GetMetadataByName(
        L"/logscrdesc/GlobalColorTableFlag",
        &propVariant);
    if (SUCCEEDED(hr))
    {
        hr = (propVariant.vt != VT_BOOL || !propVariant.boolVal) ? E_FAIL : S_OK;
        PropVariantClear(&propVariant);
    }

    if (SUCCEEDED(hr))
    {
        // Background color index
        hr = pMetadataQueryReader->GetMetadataByName(
            L"/logscrdesc/BackgroundColorIndex",
            &propVariant);
        if (SUCCEEDED(hr))
        {
            hr = (propVariant.vt != VT_UI1) ? E_FAIL : S_OK;
            if (SUCCEEDED(hr))
            {
                backgroundIndex = propVariant.bVal;
            }
            PropVariantClear(&propVariant);
        }
    }

    // Get the color from the palette
    if (SUCCEEDED(hr))
    {
        hr = ImagingFactorySingleton::GetInstance()->CreatePalette(pWicPalette.get_out_storage());
    }

    if (SUCCEEDED(hr))
    {
        // Get the global palette
        hr = decoder->CopyPalette(pWicPalette.get());
    }

    if (SUCCEEDED(hr))
    {
        hr = pWicPalette->GetColors(
            ARRAYSIZE(rgColors),
            rgColors,
            &cColorsCopied);
    }

    if (SUCCEEDED(hr))
    {
        // Check whether background color is outside range 
        hr = (backgroundIndex >= cColorsCopied) ? E_FAIL : S_OK;
    }

    if (SUCCEEDED(hr))
    {
        // Get the color in ARGB format
        dwBGColor = rgColors[backgroundIndex];

        // The background color is in ARGB format, and we want to 
        // extract the alpha value and convert it to float
        float alpha = (dwBGColor >> 24) / 255.f;
        m_backgroundColor = D2D1::ColorF(dwBGColor, alpha);
    }

    return hr;
}
