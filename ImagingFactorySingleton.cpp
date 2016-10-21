#include "ImagingFactorySingleton.h"

ComPtr<IWICImagingFactory> ImagingFactorySingleton::m_pIWICFactory;

IWICImagingFactory* ImagingFactorySingleton::GetInstance()
{
    // Create WIC factory
    if (m_pIWICFactory.get() == nullptr)
    {
        CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(m_pIWICFactory.get_out_storage()));
    }
    return m_pIWICFactory.get();
}
