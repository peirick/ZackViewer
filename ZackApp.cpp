#include <windows.h>
#include <wincodec.h>
#include <Wincodecsdk.h>
#include <commdlg.h>
#include <d2d1.h>
#include <string>
#include <vector>
#include "ZackApp.h"
#include "ImagingFactorySingleton.h"

const UINT DELAY_TIMER_ID = 1;    // Global ID for the timer, only one timer is used

// Utility inline functions

inline LONG RectWidth(RECT rc)
{
    return rc.right - rc.left;
}

inline LONG RectHeight(RECT rc)
{
    return rc.bottom - rc.top;
}

/******************************************************************
*                                                                 *
*  WinMain                                                        *
*                                                                 *
*  Application entrypoint                                         *
*                                                                 *
******************************************************************/

int WINAPI wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR pszCmdLine,
    _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(pszCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (SUCCEEDED(hr))
    {
        {
            ZackApp app;
            hr = app.Initialize(hInstance);
            if (SUCCEEDED(hr))
            {
                // Main message loop:
                MSG msg;
                while (GetMessage(&msg, nullptr, 0, 0))
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }
        }

        CoUninitialize();
    }

    return 0;
}

/******************************************************************
*                                                                 *
*  DemoApp::DemoApp constructor                                   *
*                                                                 *
*  Initializes member data                                        *
*                                                                 *
******************************************************************/

ZackApp::ZackApp() :
    m_hWnd(nullptr),
    m_pD2DFactory(nullptr),
    m_pHwndRT(nullptr),
    m_pFrameComposeRT(nullptr),
    m_pRawFrame(nullptr),
    m_pSavedFrame(nullptr),
    m_pDecoder(nullptr),
    uFrameDisposal(DM_UNDEFINED),
    uFrameDelay(0),
    m_uLoopNumber(0),
    m_uNextFrameIndex(0)
{
}

ZackApp::~ZackApp()
{

}

HRESULT ZackApp::Initialize(HINSTANCE hInstance)
{
    // Register window class
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = ZackApp::s_WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = sizeof(LONG_PTR);
    wcex.hInstance = hInstance;
    wcex.hIcon = nullptr;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = nullptr;
    wcex.lpszMenuName = MAKEINTRESOURCE(IDR_WICANIMATEDGIF);
    wcex.lpszClassName = L"ZACKVIEWER";
    wcex.hIconSm = nullptr;

    HRESULT hr = (RegisterClassEx(&wcex) == 0) ? E_FAIL : S_OK;

    if (SUCCEEDED(hr))
    {
        // Create D2D factory
        hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, m_pD2DFactory.get_out_storage());
    }

    if (SUCCEEDED(hr))
    {
        // Create window
        m_hWnd = CreateWindow(
            L"ZACKVIEWER",
            L"Zack Viewer",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            nullptr,
            nullptr,
            hInstance,
            this);

        hr = (m_hWnd == nullptr) ? E_FAIL : S_OK;
    }

    if (SUCCEEDED(hr))
    {
        SelectAndDisplayFile();
    }

    return hr;
}

/******************************************************************
*                                                                 *
*  DemoApp::CreateDeviceResources                                 *
*                                                                 *
*  Creates a D2D hwnd render target for displaying gif frames     *
*  to users and a D2D bitmap render for composing frames.         *
*                                                                 *
******************************************************************/

HRESULT ZackApp::CreateDeviceResources()
{
    HRESULT hr = S_OK;

    RECT rcClient;
    if (!GetClientRect(m_hWnd, &rcClient))
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
    }

    if (SUCCEEDED(hr))
    {
        if (m_pHwndRT.get() == nullptr)
        {
            auto renderTargetProperties = D2D1::RenderTargetProperties();

            // Set the DPI to be the default system DPI to allow direct mapping
            // between image pixels and desktop pixels in different system DPI settings
            renderTargetProperties.dpiX = DEFAULT_DPI;
            renderTargetProperties.dpiY = DEFAULT_DPI;

            auto hwndRenderTargetproperties
                = D2D1::HwndRenderTargetProperties(m_hWnd,
                    D2D1::SizeU(RectWidth(rcClient), RectHeight(rcClient)));

            hr = m_pD2DFactory->CreateHwndRenderTarget(
                renderTargetProperties,
                hwndRenderTargetproperties,
                m_pHwndRT.get_out_storage());
        }
        else
        {
            // We already have a hwnd render target, resize it to the window size
            D2D1_SIZE_U size;
            size.width = RectWidth(rcClient);
            size.height = RectHeight(rcClient);
            hr = m_pHwndRT->Resize(size);
        }
    }

    if (SUCCEEDED(hr))
    {
        // Create a bitmap render target used to compose frames. Bitmap render 
        // targets cannot be resized, so we always recreate it.		
        m_pFrameComposeRT.reset(nullptr);
        hr = m_pHwndRT->CreateCompatibleRenderTarget(
            D2D1::SizeF(
                static_cast<float>(m_imageInfo.getImageWidth()),
                static_cast<float>(m_imageInfo.getImageHeight())),
            m_pFrameComposeRT.get_out_storage());
    }

    return hr;
}

/******************************************************************
*                                                                 *
*  DemoApp::OnRender                                              *
*                                                                 *
*  Called whenever the application needs to display the client    *
*  window.                                                        *
*                                                                 *
*  Renders the pre-composed frame by drawing it onto the hwnd     *
*  render target.                                                 *
*                                                                 *
******************************************************************/

HRESULT ZackApp::OnRender()
{


    // Check to see if the render targets are initialized
    if (!m_pHwndRT.get() || !m_pFrameComposeRT.get())
        return S_OK;


    // Only render when the window is not occluded
    if ((m_pHwndRT->CheckWindowState() & D2D1_WINDOW_STATE_OCCLUDED))
        return S_OK;


    D2D1_RECT_F drawRect;
    HRESULT hr = CalculateDrawRectangle(drawRect);
    if (FAILED(hr))
        return hr;

    // Get the bitmap to draw on the hwnd render target
    ComPtr<ID2D1Bitmap> pFrameToRender;
    hr = m_pFrameComposeRT->GetBitmap(pFrameToRender.get_out_storage());
    if (FAILED(hr))
        return hr;

    // Draw the bitmap onto the calculated rectangle
    m_pHwndRT->BeginDraw();

    m_pHwndRT->Clear(D2D1::ColorF(D2D1::ColorF::Black));
    m_pHwndRT->DrawBitmap(pFrameToRender.get(), drawRect);

    return m_pHwndRT->EndDraw();
}

/******************************************************************
*                                                                 *
*  DemoApp::GetFileOpen                                           *
*                                                                 *
*  Creates an open file dialog box and returns the filename       *
*  of the file selected(if any).                                  *
*                                                                 *
******************************************************************/

#define READ_WIC_STRING(f, out) do {            \
    UINT strLen = 0;                            \
	HRESULT result = f(0, 0, &strLen);          \
    if (SUCCEEDED(result) && (strLen > 0)) {    \
		out.resize(strLen);						\
        result = f(strLen, &out[0], &strLen);   \
        out.resize(strLen - 1);                 \
    } else { out = L""; } } while(0);

bool ZackApp::OpenImageFile(WCHAR *pszFileName, DWORD cchFileName) const
{
    pszFileName[0] = L'\0';

    OPENFILENAME ofn;
    RtlZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hWnd;
    ofn.lpstrFilter = L"All files (*.*)\0*.*\0";
    ofn.lpstrFile = pszFileName;
    ofn.nMaxFile = cchFileName;
    ofn.lpstrTitle = L"Open Image";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    // Display the Open dialog box.
    return (GetOpenFileName(&ofn) == TRUE);
}

void replaceAll(std::wstring& str, const std::wstring& from, const std::wstring& to) {
    if (from.empty())
        return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::wstring::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
}

bool ZackApp::GetFileSave(WCHAR *pszFileName, DWORD cchFileName, GUID& containerformat) const
{
    pszFileName[0] = L'\0';

    HRESULT hr = S_OK;
    ComPtr<IEnumUnknown> e;
    hr = ImagingFactorySingleton::GetInstance()->CreateComponentEnumerator(
        WICEncoder, WICComponentEnumerateRefresh, e.get_out_storage());
    if (FAILED(hr))
        return false;
    ULONG num = 0;
    ComPtr<IUnknown> unk;
    std::vector<std::wstring> encoderNames;
    std::vector<std::wstring> fileExtensions;
    std::vector<GUID> containerFormats;
    while ((S_OK == e->Next(1, unk.get_out_storage(), &num)) && (1 == num))
    {
        ComPtr<IWICBitmapEncoderInfo> encoderInfo;
        hr = unk->QueryInterface(encoderInfo.get_out_storage());
        if (FAILED(hr))
            return false;

        // Get the name of the container
        std::wstring encoderName;
        READ_WIC_STRING(encoderInfo->GetFriendlyName, encoderName);

        std::wstring fileExtension;
        READ_WIC_STRING(encoderInfo->GetFileExtensions, fileExtension);
        replaceAll(fileExtension, L".", L"*.");
        replaceAll(fileExtension, L",", L";");

        GUID containerFormat = { 0 };
        hr = encoderInfo->GetContainerFormat(&containerFormat);
        if (FAILED(hr))
            return false;

        encoderNames.emplace_back(encoderName);
        fileExtensions.emplace_back(fileExtension);
        containerFormats.emplace_back(containerFormat);
    }

    std::wstring filter;
    for (int i = 0; i < encoderNames.size(); ++i) {
        filter += encoderNames[i] + L" (" + fileExtensions[i] + L")" + L'\0' + fileExtensions[i] + L'\0';
    }
    filter += L'\0';

    OPENFILENAME ofn;
    RtlZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hWnd;
    ofn.lpstrFilter = filter.c_str();
    ofn.lpstrFile = pszFileName;
    ofn.nMaxFile = cchFileName;
    ofn.lpstrTitle = L"Save as";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    // Display the Open dialog box.
    if (GetSaveFileName(&ofn) == TRUE) {
        if (ofn.nFilterIndex > 0 && ofn.nFilterIndex <= containerFormats.size()) {
            containerformat = containerFormats[ofn.nFilterIndex - 1];
            return true;
        }
    }
    return false;
}

/******************************************************************
*                                                                 *
*  DemoApp::OnResize                                              *
*                                                                 *
*  If the application receives a WM_SIZE message, this method     *
*  will resize the render target appropriately.                   *
*                                                                 *
******************************************************************/

HRESULT ZackApp::OnResize(UINT uWidth, UINT uHeight)
{
    HRESULT hr = S_OK;

    if (!m_pHwndRT.get())
        return S_OK;

    D2D1_SIZE_U size;
    size.width = uWidth;
    size.height = uHeight;
    return m_pHwndRT->Resize(size);
}

/******************************************************************
*                                                                 *
*  DemoApp::s_WndProc                                             *
*                                                                 *
*  Static window message handler used to initialize the           *
*  application object and call the object's member WndProc        *
*                                                                 *
******************************************************************/

LRESULT CALLBACK ZackApp::s_WndProc(
    HWND hWnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    ZackApp *pThis = nullptr;
    LRESULT lRet = 0;

    if (uMsg == WM_NCCREATE)
    {
        auto pcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
        pThis = reinterpret_cast<ZackApp *>(pcs->lpCreateParams);

        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR> (pThis));
        lRet = DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    else
    {
        pThis = reinterpret_cast<ZackApp *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
        if (pThis)
        {
            lRet = pThis->WndProc(hWnd, uMsg, wParam, lParam);
        }
        else
        {
            lRet = DefWindowProc(hWnd, uMsg, wParam, lParam);
        }
    }

    return lRet;
}


/******************************************************************
*                                                                 *
*  DemoApp::WndProc                                               *
*                                                                 *
*  Window message handler                                         *
*                                                                 *
******************************************************************/

bool ZackApp::ShowFirstPage()
{
    if (m_imageInfo.getFrameCount() > 1 && uFrameDelay == 0 && m_uNextFrameIndex > 0) {
        m_uNextFrameIndex = 0;
        ComposeNextFrame();
        InvalidateRect(m_hWnd, nullptr, FALSE);
        return true;
    }
    return false;
}

bool ZackApp::ShowLastPage()
{
    if (m_imageInfo.getFrameCount() > 1 && uFrameDelay == 0 && m_uNextFrameIndex < m_imageInfo.getFrameCount() - 1) {
        m_uNextFrameIndex = m_imageInfo.getFrameCount() - 1;
        ComposeNextFrame();
        InvalidateRect(m_hWnd, nullptr, FALSE);
        return true;
    }
    return false;
}

bool ZackApp::ShowNextPage()
{
    if (m_imageInfo.getFrameCount() > 1 && uFrameDelay == 0 && m_uNextFrameIndex < m_imageInfo.getFrameCount() - 1) {
        ++m_uNextFrameIndex;
        ComposeNextFrame();
        InvalidateRect(m_hWnd, nullptr, FALSE);
        UpdateWindow(m_hWnd);
        return true;
    }
    return false;
}

bool ZackApp::ShowPreviousPage()
{
    if (m_imageInfo.getFrameCount() > 1 && uFrameDelay == 0 && m_uNextFrameIndex > 0) {
        --m_uNextFrameIndex;
        ComposeNextFrame();
        InvalidateRect(m_hWnd, nullptr, FALSE);
        UpdateWindow(m_hWnd);
        return true;
    }
    return false;
}

LRESULT ZackApp::WndProc(
    HWND hWnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    HRESULT hr = S_OK;

    switch (uMsg)
    {
    case WM_COMMAND:
    {
        // Parse the menu selections
        switch (LOWORD(wParam))
        {
        case IDM_FILE_OPEN:
            SelectAndDisplayFile();
            if (FAILED(hr))
            {
                MessageBox(hWnd, L"Load of image file failed.", L"Error", MB_OK);
            }
            break;

        case IDM_FILE_SAVE:
            SelectAndSaveFile();
            break;

        case IDM_EXIT:
            PostMessage(hWnd, WM_CLOSE, 0, 0);
            break;
        }
    }
    break;

    case WM_KEYDOWN:
    {
        switch (wParam)
        {
        case VK_HOME:
            if (ShowFirstPage())
                return 0;
            break;
        case VK_END:
            if (ShowLastPage())
                return 0;
            break;
        case VK_NEXT:
            if (ShowNextPage())
                return 0;
            break;
        case VK_PRIOR:
            if (ShowPreviousPage())
                return 0;
            break;
        }
    }
    break;

    case WM_SIZE:
    {
        UINT uWidth = LOWORD(lParam);
        UINT uHeight = HIWORD(lParam);
        hr = OnResize(uWidth, uHeight);
    }
    break;

    case WM_PAINT:
    {
        hr = OnRender();
        ValidateRect(hWnd, nullptr);
    }
    break;

    case WM_DISPLAYCHANGE:
    {
        InvalidateRect(hWnd, nullptr, FALSE);
    }
    break;

    case WM_DESTROY:
    {
        PostQuitMessage(0);
        return 0;
    }
    break;

    case WM_TIMER:
    {
        // Timer expired, display the next frame and set a new timer
        // if needed
        hr = ComposeNextFrame();
        InvalidateRect(hWnd, nullptr, FALSE);
    }
    break;

    default:
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    // In case of a device loss, recreate all the resources and start playing
    // gif from the beginning
    //
    // In case of other errors from resize, paint, and timer event, we will
    // try our best to continue displaying the animation
    if (hr == D2DERR_RECREATE_TARGET)
    {
        hr = RecoverDeviceResources();
        if (FAILED(hr))
        {
            MessageBox(hWnd, L"Device loss recovery failed. Exiting application.", L"Error", MB_OK);
            PostQuitMessage(1);
        }
    }

    return 0;
}


/******************************************************************
*                                                                 *
*  DemoApp::GetRawFrame()                                         *
*                                                                 *
*  Decodes the current raw frame, retrieves its timing            *
*  information, disposal method, and frame dimension for          *
*  rendering.  Raw frame is the frame read directly from the gif  *
*  file without composing.                                        *
*                                                                 *
******************************************************************/

HRESULT ZackApp::GetRawFrame(UINT uFrameIndex)
{
    ComPtr<IWICFormatConverter> pConverter;
    ComPtr<IWICBitmapFrameDecode> pWicFrame;

    PROPVARIANT propValue;
    PropVariantInit(&propValue);

    // Retrieve the current frame
    HRESULT hr = m_pDecoder->GetFrame(uFrameIndex, pWicFrame.get_out_storage());
    if (SUCCEEDED(hr))
    {
        // Format convert to 32bppPBGRA which D2D expects
        hr = ImagingFactorySingleton::GetInstance()->CreateFormatConverter(pConverter.get_out_storage());
    }

    if (SUCCEEDED(hr))
    {
        hr = pConverter->Initialize(
            pWicFrame.get(),
            GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.f,
            WICBitmapPaletteTypeCustom);
    }

    if (SUCCEEDED(hr))
    {
        // Create a D2DBitmap from IWICBitmapSource
        m_pRawFrame.reset(nullptr);
        hr = m_pHwndRT->CreateBitmapFromWicBitmap(
            pConverter.get(),
            nullptr,
            m_pRawFrame.get_out_storage());
    }

    m_framePosition.left = 0;
    m_framePosition.top = 0;
    m_framePosition.right = static_cast<float>(m_imageInfo.getImageWidthPixel());
    m_framePosition.bottom = static_cast<float>(m_imageInfo.getImageHeightPixel());
    uFrameDelay = 0;
    uFrameDisposal = DM_UNDEFINED;

    HRESULT metadatareaderresult;
    if (SUCCEEDED(hr))
    {

        ComPtr<IWICMetadataQueryReader> pFrameMetadataQueryReader;

        // Get Metadata Query Reader from the frame
        metadatareaderresult = pWicFrame->GetMetadataQueryReader(pFrameMetadataQueryReader.get_out_storage());


        // Get the Metadata for the current frame
        if (SUCCEEDED(metadatareaderresult))
        {
            metadatareaderresult = pFrameMetadataQueryReader->GetMetadataByName(L"/imgdesc/Left", &propValue);
            if (SUCCEEDED(metadatareaderresult))
            {
                metadatareaderresult = (propValue.vt == VT_UI2 ? S_OK : E_FAIL);
                if (SUCCEEDED(hr))
                {
                    m_framePosition.left = static_cast<float>(propValue.uiVal);
                }
                PropVariantClear(&propValue);
            }
        }

        if (SUCCEEDED(metadatareaderresult))
        {
            metadatareaderresult = pFrameMetadataQueryReader->GetMetadataByName(L"/imgdesc/Top", &propValue);
            if (SUCCEEDED(metadatareaderresult))
            {
                metadatareaderresult = (propValue.vt == VT_UI2 ? S_OK : E_FAIL);
                if (SUCCEEDED(hr))
                {
                    m_framePosition.top = static_cast<float>(propValue.uiVal);
                }
                PropVariantClear(&propValue);
            }
        }

        if (SUCCEEDED(metadatareaderresult))
        {
            metadatareaderresult = pFrameMetadataQueryReader->GetMetadataByName(L"/imgdesc/Width", &propValue);
            if (SUCCEEDED(metadatareaderresult))
            {
                metadatareaderresult = (propValue.vt == VT_UI2 ? S_OK : E_FAIL);
                if (SUCCEEDED(hr))
                {
                    m_framePosition.right = static_cast<float>(propValue.uiVal)
                        + m_framePosition.left;
                }
                PropVariantClear(&propValue);
            }
        }

        if (SUCCEEDED(metadatareaderresult))
        {
            metadatareaderresult = pFrameMetadataQueryReader->GetMetadataByName(L"/imgdesc/Height", &propValue);
            if (SUCCEEDED(metadatareaderresult))
            {
                metadatareaderresult = (propValue.vt == VT_UI2 ? S_OK : E_FAIL);
                if (SUCCEEDED(hr))
                {
                    m_framePosition.bottom = static_cast<float>(propValue.uiVal)
                        + m_framePosition.top;
                }
                PropVariantClear(&propValue);
            }
        }

        if (SUCCEEDED(metadatareaderresult))
        {
            // Get delay from the optional Graphic Control Extension
            if (SUCCEEDED(pFrameMetadataQueryReader->GetMetadataByName(
                L"/grctlext/Delay",
                &propValue)))
            {
                metadatareaderresult = (propValue.vt == VT_UI2 ? S_OK : E_FAIL);
                if (SUCCEEDED(hr))
                {
                    // Convert the delay retrieved in 10 ms units to a delay in 1 ms units
                    metadatareaderresult = UIntMult(propValue.uiVal, 10, &uFrameDelay);
                }
                PropVariantClear(&propValue);
            }
            else
            {
                // Failed to get delay from graphic control extension. Possibly a
                // single frame image (non-animated gif)
                uFrameDelay = 0;
            }

            if (SUCCEEDED(metadatareaderresult))
            {
                // Insert an artificial delay to ensure rendering for gif with very small
                // or 0 delay.  This delay number is picked to match with most browsers' 
                // gif display speed.
                //
                // This will defeat the purpose of using zero delay intermediate frames in 
                // order to preserve compatibility. If this is removed, the zero delay 
                // intermediate frames will not be visible.
                if (uFrameDelay < 20)
                {
                    uFrameDelay = 20;
                }
            }
        }

        if (SUCCEEDED(metadatareaderresult))
        {
            if (SUCCEEDED(pFrameMetadataQueryReader->GetMetadataByName(
                L"/grctlext/Disposal",
                &propValue)))
            {
                metadatareaderresult = (propValue.vt == VT_UI1) ? S_OK : E_FAIL;
                if (SUCCEEDED(hr))
                {
                    uFrameDisposal = (DISPOSAL_METHODS)propValue.bVal;
                }
            }
            else
            {
                // Failed to get the disposal method, use default. Possibly a 
                // non-animated gif.
                uFrameDisposal = DM_UNDEFINED;
            }
        }
    }
    PropVariantClear(&propValue);
    return hr;
}


/******************************************************************
*                                                                 *
*  DemoApp::CalculateDrawRectangle()                              *
*                                                                 *
*  Calculates a specific rectangular area of the hwnd             *
*  render target to draw a bitmap containing the current          *
*  composed frame.                                                *
*                                                                 *
******************************************************************/

bool ZackApp::EndOfAnimation() const
{
    return m_imageInfo.hasLoop() && IsLastFrame() && m_uLoopNumber == m_imageInfo.getTotalLoopCount() + 1;
}

HRESULT ZackApp::CalculateDrawRectangle(D2D1_RECT_F &drawRect) const
{
    HRESULT hr = S_OK;
    RECT rcClient;

    // Top and left of the client rectangle are both 0
    if (!GetClientRect(m_hWnd, &rcClient))
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
    }

    if (SUCCEEDED(hr))
    {
        // Calculate the area to display the image
        // Center the image if the client rectangle is larger
        drawRect.left = (static_cast<float>(rcClient.right) - m_imageInfo.getImageWidthPixel()) / 2.f;
        drawRect.top = (static_cast<float>(rcClient.bottom) - m_imageInfo.getImageHeightPixel()) / 2.f;
        drawRect.right = drawRect.left + m_imageInfo.getImageWidthPixel();
        drawRect.bottom = drawRect.top + m_imageInfo.getImageHeightPixel();

        // If the client area is resized to be smaller than the image size, scale
        // the image, and preserve the aspect ratio
        auto aspectRatio = static_cast<float>(m_imageInfo.getImageWidthPixel()) /
            static_cast<float>(m_imageInfo.getImageHeightPixel());

        if (drawRect.left < 0)
        {
            auto newWidth = static_cast<float>(rcClient.right);
            float newHeight = newWidth / aspectRatio;
            drawRect.left = 0;
            drawRect.top = (static_cast<float>(rcClient.bottom) - newHeight) / 2.f;
            drawRect.right = newWidth;
            drawRect.bottom = drawRect.top + newHeight;
        }

        if (drawRect.top < 0)
        {
            auto newHeight = static_cast<float>(rcClient.bottom);
            float newWidth = newHeight * aspectRatio;
            drawRect.left = (static_cast<float>(rcClient.right) - newWidth) / 2.f;
            drawRect.top = 0;
            drawRect.right = drawRect.left + newWidth;
            drawRect.bottom = newHeight;
        }
    }

    return hr;
}

/******************************************************************
*                                                                 *
*  DemoApp::RestoreSavedFrame()                                   *
*                                                                 *
*  Copys the saved frame to the frame in the bitmap render        *
*  target.                                                        *
*                                                                 *
******************************************************************/

HRESULT ZackApp::RestoreSavedFrame()
{
    HRESULT hr = S_OK;

    ComPtr<ID2D1Bitmap> pFrameToCopyTo;

    hr = m_pSavedFrame.get() ? S_OK : E_FAIL;

    if (SUCCEEDED(hr))
    {
        hr = m_pFrameComposeRT->GetBitmap(pFrameToCopyTo.get_out_storage());
    }

    if (SUCCEEDED(hr))
    {
        // Copy the whole bitmap
        hr = pFrameToCopyTo->CopyFromBitmap(nullptr, m_pSavedFrame.get(), nullptr);
    }

    return hr;
}

/******************************************************************
*                                                                 *
*  DemoApp::ClearCurrentFrameArea()                               *
*                                                                 *
*  Clears a rectangular area equal to the area overlaid by the    *
*  current raw frame in the bitmap render target with background  *
*  color.                                                         *
*                                                                 *
******************************************************************/

HRESULT ZackApp::ClearCurrentFrameArea()
{
    m_pFrameComposeRT->BeginDraw();

    // Clip the render target to the size of the raw frame
    m_pFrameComposeRT->PushAxisAlignedClip(
        &m_framePosition,
        D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    m_pFrameComposeRT->Clear(m_imageInfo.getBackgroundColor());

    // Remove the clipping
    m_pFrameComposeRT->PopAxisAlignedClip();

    return m_pFrameComposeRT->EndDraw();
}

bool ZackApp::IsLastFrame() const
{
    return (m_uNextFrameIndex == 0);
}

/******************************************************************
*                                                                 *
*  DemoApp::DisposeCurrentFrame()                                 *
*                                                                 *
*  At the end of each delay, disposes the current frame           *
*  based on the disposal method specified.                        *
*                                                                 *
******************************************************************/

HRESULT ZackApp::DisposeCurrentFrame()
{
    HRESULT hr = S_OK;

    switch (uFrameDisposal)
    {
    case DM_UNDEFINED:
    case DM_NONE:
        // We simply draw on the previous frames. Do nothing here.
        break;
    case DM_BACKGROUND:
        // Dispose background
        // Clear the area covered by the current raw frame with background color
        hr = ClearCurrentFrameArea();
        break;
    case DM_PREVIOUS:
        // Dispose previous
        // We restore the previous composed frame first
        hr = RestoreSavedFrame();
        break;
    default:
        // Invalid disposal method
        hr = E_FAIL;
    }

    return hr;
}

/******************************************************************
*                                                                 *
*  DemoApp::OverlayNextFrame()                                    *
*                                                                 *
*  Loads and draws the next raw frame into the composed frame     *
*  render target. This is called after the current frame is       *
*  disposed.                                                      *
*                                                                 *
******************************************************************/

HRESULT ZackApp::OverlayNextFrame()
{
    // Get Frame information
    HRESULT hr = GetRawFrame(m_uNextFrameIndex);
    if (SUCCEEDED(hr))
    {
        // For disposal 3 method, we would want to save a copy of the current
        // composed frame
        if (uFrameDisposal == DM_PREVIOUS)
        {
            hr = SaveComposedFrame();
        }
    }

    if (SUCCEEDED(hr))
    {
        // Start producing the next bitmap
        m_pFrameComposeRT->BeginDraw();

        // If starting a new animation loop
        if (m_uNextFrameIndex == 0)
        {
            // Draw background and increase loop count
            m_pFrameComposeRT->Clear(m_imageInfo.getBackgroundColor());
            m_uLoopNumber++;
        }

        // Produce the next frame
        m_pFrameComposeRT->DrawBitmap(
            m_pRawFrame.get(),
            m_framePosition);

        hr = m_pFrameComposeRT->EndDraw();
    }

    // To improve performance and avoid decoding/composing this frame in the 
    // following animation loops, the composed frame can be cached here in system 
    // or video memory.
    return hr;
}

/******************************************************************
*                                                                 *
*  DemoApp::SaveComposedFrame()                                   *
*                                                                 *
*  Saves the current composed frame in the bitmap render target   *
*  into a temporary bitmap. Initializes the temporary bitmap if   *
*  needed.                                                        *
*                                                                 *
******************************************************************/

HRESULT ZackApp::SaveComposedFrame()
{
    HRESULT hr = S_OK;
    ComPtr<ID2D1Bitmap> pFrameToBeSaved;
    hr = m_pFrameComposeRT->GetBitmap(pFrameToBeSaved.get_out_storage());
    if (SUCCEEDED(hr))
    {
        // Create the temporary bitmap if it hasn't been created yet 
        if (m_pSavedFrame.get() == nullptr)
        {
            auto bitmapSize = pFrameToBeSaved->GetPixelSize();
            D2D1_BITMAP_PROPERTIES bitmapProp;
            pFrameToBeSaved->GetDpi(&bitmapProp.dpiX, &bitmapProp.dpiY);
            bitmapProp.pixelFormat = pFrameToBeSaved->GetPixelFormat();

            hr = m_pFrameComposeRT->CreateBitmap(
                bitmapSize,
                bitmapProp,
                m_pSavedFrame.get_out_storage());
        }
    }
    if (SUCCEEDED(hr))
    {
        // Copy the whole bitmap
        hr = m_pSavedFrame->CopyFromBitmap(nullptr, pFrameToBeSaved.get(), nullptr);
    }
    return hr;
}


HRESULT ZackApp::SelectAndDisplayFile()
{
    HRESULT hr = S_OK;

    WCHAR szFileName[MAX_PATH] = {};
    RECT rcClient = {};
    RECT rcWindow = {};

    // If the user cancels selection, then nothing happens
    if (OpenImageFile(szFileName, ARRAYSIZE(szFileName)))
    {
        WCHAR newCaption[MAX_PATH] = {};
        WCHAR fileName[MAX_PATH] = {};
        WCHAR fileExt[MAX_PATH] = {};
        _wsplitpath_s(szFileName, nullptr, 0, nullptr, 0, fileName, MAX_PATH, fileExt, MAX_PATH);
        swprintf_s(newCaption, L"Zack Viewer (\"%s%s\")", fileName, fileExt);
        SetWindowText(m_hWnd, newCaption);


        // Reset the states
        m_uNextFrameIndex = 0;
        uFrameDisposal = DM_NONE;  // No previous frame, use disposal none
        m_uLoopNumber = 0;
        m_imageInfo.Reset();
        m_pSavedFrame.reset(nullptr);

        // Create a decoder for the gif file
        m_pDecoder.reset(nullptr);
        hr = ImagingFactorySingleton::GetInstance()->CreateDecoderFromFilename(
            szFileName,
            nullptr,
            GENERIC_READ,
            WICDecodeMetadataCacheOnLoad,
            m_pDecoder.get_out_storage());
        if (FAILED(hr))
            return hr;


        if (FAILED(m_imageInfo.GetGlobalMetadata(m_pDecoder.get())))
        {
            hr = m_imageInfo.GetDefaultMetadata(m_pDecoder.get());
            if (FAILED(hr))
                return hr;
        }

        rcClient.right = m_imageInfo.getImageWidthPixel();
        rcClient.bottom = m_imageInfo.getImageHeightPixel();

        if (!AdjustWindowRect(&rcClient, WS_OVERLAPPEDWINDOW, TRUE))
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            if (FAILED(hr))
                return hr;
        }

        // Get the upper left corner of the current window
        if (!GetWindowRect(m_hWnd, &rcWindow))
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            if (FAILED(hr))
                return hr;
        }

        // Resize the window to fit the gif
   /*     MoveWindow(
            m_hWnd,
            rcWindow.left,
            rcWindow.top,
            RectWidth(rcClient),
            RectHeight(rcClient),
            TRUE);*/

        hr = CreateDeviceResources();
        if (FAILED(hr))
            return hr;


        // If we have at least one frame, start playing
        // the animation from the first frame
        if (m_imageInfo.getFrameCount() > 0)
        {
            hr = ComposeNextFrame();
            InvalidateRect(m_hWnd, nullptr, FALSE);
        }
    }

    return hr;
}

HRESULT ZackApp::SelectAndSaveFile() {
    if (m_pDecoder.get() == nullptr)
        return S_FALSE;

    HRESULT hr = S_OK;

    WCHAR szFileName[MAX_PATH];
    GUID containerformat = { 0 };
    // If the user cancels selection, then nothing happens
    if (GetFileSave(szFileName, ARRAYSIZE(szFileName), containerformat))
    {
        ComPtr<IWICBitmapEncoder> encoder;
        ComPtr<IWICStream> stream;
        ComPtr<IWICBitmapFrameEncode> frameEncode;
        ComPtr<IWICBitmapFrameDecode> frameDecode;
        ComPtr<IWICMetadataBlockWriter> blockWriter;
        ComPtr<IWICMetadataBlockReader> blockReader;

        hr = ImagingFactorySingleton::GetInstance()->CreateStream(stream.get_out_storage());
        if (FAILED(hr))
            return hr;

        hr = stream->InitializeFromFilename(szFileName, GENERIC_WRITE);
        if (FAILED(hr))
            return hr;

        hr = ImagingFactorySingleton::GetInstance()->CreateEncoder(containerformat, nullptr, encoder.get_out_storage());
        if (FAILED(hr))
            return hr;

        hr = encoder->Initialize(stream.get(), WICBitmapEncoderNoCache);
        if (FAILED(hr))
            return hr;

        hr = encoder->CreateNewFrame(frameEncode.get_out_storage(), nullptr);
        if (FAILED(hr))
            return hr;

        hr = frameEncode->Initialize(nullptr);
        if (FAILED(hr))
            return hr;

        UINT frameCount = 0;
        hr = m_pDecoder->GetFrameCount(&frameCount);
        if (FAILED(hr))
            return hr;
        for (UINT i = 0; i < frameCount; ++i) {
            hr = m_pDecoder->GetFrame(i, frameDecode.get_out_storage());
            if (FAILED(hr))
                return hr;

            if (SUCCEEDED(frameDecode->QueryInterface(blockReader.get_out_storage())) &&
                SUCCEEDED(frameEncode->QueryInterface(blockWriter.get_out_storage())))
            {
                blockWriter->InitializeFromBlockReader(blockReader.get());
            }

            hr = frameEncode->WriteSource(frameDecode.get(), nullptr);
            if (FAILED(hr))
                return hr;

            hr = frameEncode->Commit();
            if (FAILED(hr))
                return hr;
        }

        hr = encoder->Commit();
        if (FAILED(hr))
            return hr;
    }
    return hr;
}

/******************************************************************
*                                                                 *
*  DemoApp::ComposeNextFrame()                                    *
*                                                                 *
*  Composes the next frame by first disposing the current frame   *
*  and then overlaying the next frame. More than one frame may    *
*  be processed in order to produce the next frame to be          *
*  displayed due to the use of zero delay intermediate frames.    *
*  Also, sets a timer that is equal to the delay of the frame.    *
*                                                                 *
******************************************************************/

HRESULT ZackApp::ComposeNextFrame()
{
    HRESULT hr = S_OK;

    // Check to see if the render targets are initialized
    if (m_pHwndRT.get() && m_pFrameComposeRT.get())
    {
        // First, kill the timer since the delay is no longer valid
        KillTimer(m_hWnd, DELAY_TIMER_ID);

        // Compose one frame
        hr = DisposeCurrentFrame();
        if (SUCCEEDED(hr))
        {
            hr = OverlayNextFrame();
        }

        // If we have more frames to play, set the timer according to the delay.
        // Set the timer regardless of whether we succeeded in composing a frame
        // to try our best to continue displaying the animation.
        if (!EndOfAnimation() && m_imageInfo.getFrameCount() > 1 && uFrameDelay > 0)
        {
            // Increase the frame index by 1
            m_uNextFrameIndex = (++m_uNextFrameIndex) % m_imageInfo.getFrameCount();

            // Set the timer according to the delay
            SetTimer(m_hWnd, DELAY_TIMER_ID, uFrameDelay, nullptr);
        }
    }

    return hr;
}

/******************************************************************
*                                                                 *
*  DemoApp::RecoverDeviceResources                                *
*                                                                 *
*  Discards device-specific resources and recreates them.         *
*  Also starts the animation from the beginning.                  *
*                                                                 *
******************************************************************/

HRESULT ZackApp::RecoverDeviceResources()
{
    m_pHwndRT.reset(nullptr);
    m_pFrameComposeRT.reset(nullptr);
    m_pSavedFrame.reset(nullptr);

    m_uNextFrameIndex = 0;
    uFrameDisposal = DM_NONE;  // No previous frames. Use disposal none.
    m_uLoopNumber = 0;

    HRESULT hr = CreateDeviceResources();
    if (SUCCEEDED(hr))
    {
        if (m_imageInfo.getFrameCount() > 0)
        {
            // Load the first frame
            hr = ComposeNextFrame();
            InvalidateRect(m_hWnd, nullptr, FALSE);
        }
    }

    return hr;
}