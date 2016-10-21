// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved


#pragma once

#include "resource.h"
#include "ComPtr.h"
#include "ImageInfo.h"

const float DEFAULT_DPI = 96.f;   // Default DPI that maps image resolution directly to screen resoltuion

enum DISPOSAL_METHODS
{
    DM_UNDEFINED = 0,
    DM_NONE = 1,
    DM_BACKGROUND = 2,
    DM_PREVIOUS = 3
};


class ZackApp
{
public:

    ZackApp();
    ~ZackApp();

    HRESULT Initialize(HINSTANCE hInstance);

private:



    HRESULT CreateDeviceResources();
    HRESULT RecoverDeviceResources();

    HRESULT OnResize(UINT uWidth, UINT uHeight);
    HRESULT OnRender();

    bool    OpenImageFile(WCHAR * pszFileName, DWORD cchFileName) const;
    bool	GetFileSave(WCHAR * pszFileName, DWORD cchFileName, GUID& containerformat) const;
    HRESULT SelectAndDisplayFile();
    HRESULT SelectAndSaveFile();

    HRESULT GetRawFrame(UINT uFrameIndex);

    HRESULT ComposeNextFrame();
    HRESULT DisposeCurrentFrame();
    HRESULT OverlayNextFrame();

    HRESULT SaveComposedFrame();
    HRESULT RestoreSavedFrame();
    HRESULT ClearCurrentFrameArea();

    bool IsLastFrame() const;

    bool EndOfAnimation() const;

    HRESULT CalculateDrawRectangle(D2D1_RECT_F &drawRect) const;

    LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK s_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    bool ShowFirstPage();
    bool ShowLastPage();
    bool ShowNextPage();
    bool ShowPreviousPage();
private:

    HWND                        m_hWnd;

    ComPtr<ID2D1Factory>             m_pD2DFactory;
    ComPtr<ID2D1HwndRenderTarget>    m_pHwndRT;
    ComPtr<ID2D1BitmapRenderTarget>  m_pFrameComposeRT;
    ComPtr<ID2D1Bitmap>              m_pRawFrame;
    ComPtr<ID2D1Bitmap>              m_pSavedFrame;          // The temporary bitmap used for disposal 3 method
    ComPtr<IWICBitmapDecoder>        m_pDecoder;

    DISPOSAL_METHODS uFrameDisposal;
    unsigned int     uFrameDelay;

    ImageInfo       m_imageInfo;
    unsigned int    m_uLoopNumber;      // The current animation loop number (e.g. 1 when the animation is first played)
    unsigned int    m_uNextFrameIndex;
    D2D1_RECT_F     m_framePosition;

};

