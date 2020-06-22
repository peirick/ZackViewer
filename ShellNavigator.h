#pragma once
#include "ComPtr.h"
#include <shobjidl.h>
#include <deque>
#include <string>

class ShellNavigator
{
public:
    ShellNavigator();
    ~ShellNavigator();
    void Reset(IShellItem* shellItem);
    bool GetNext(IShellItem** shellItem);
    bool GetPrevious(IShellItem** shellItem);
private:
    ShellNavigator(const ShellNavigator&) = delete;
    ShellNavigator& operator=(const ShellNavigator&) = delete;

    std::wstring                   m_path;
    std::deque<ComPtr<IShellItem>> m_files;
    size_t                         m_index;
};

