# ZackViewer
Very Basic application that allows to **show** and **save** files using the Windows Imaging Component (WIC) API.
It shows animated GIF and, if the FlifWICCodec(https://github.com/peirick/FlifWICCodec) is installed, animated FLIF files. 
It also supports JPEG files and multipage TIFF.

It is based on [Windows Imaging Component Animated GIF Win32 Sample](https://code.msdn.microsoft.com/windowsapps/Windows-Imaging-Component-65abbc6a)

## Help

Use your keyboard keys *PageUp* and *PageDown* to navigate between pages in a multipage TIFF.
Use your keyboard keys 'Home* and *End* to navigate to the first or last page of a multipage TIFF.

## Install WIC-Codecs to get support for more image formats

* Flif: [https://github.com/peirick/FlifWICCodec](https://github.com/peirick/FlifWICCodec)
* WebP: [https://developers.google.com/speed/webp/docs/webp_codec](https://developers.google.com/speed/webp/docs/webp_codec)

## Build Instructions

1. Open Visual Studio 2015 and open ZackViewer.sln
2. Compile
