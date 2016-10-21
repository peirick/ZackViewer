#pragma once
#include <wincodec.h>
#include "ComPtr.h"

class ImagingFactorySingleton
{
public:
    static IWICImagingFactory* GetInstance();
private:
    ImagingFactorySingleton(const ImagingFactorySingleton&) = delete;
    ImagingFactorySingleton& operator=(const ImagingFactorySingleton&) = delete;
    ImagingFactorySingleton() = delete;   // Private because singleton
    static  ComPtr<IWICImagingFactory> m_pIWICFactory;
};


