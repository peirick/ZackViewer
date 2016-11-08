#include "ShellNavigator.h"
#include <shlobj.h>
#include "ComPtr.h"


ShellNavigator::ShellNavigator() : m_index(0)
{
}


ShellNavigator::~ShellNavigator()
{
}

void ShellNavigator::Reset(IShellItem* shellItem)
{
    m_index = 0;
    m_files.clear();
    ComPtr<IShellItem> parent;
    if (SUCCEEDED(shellItem->GetParent(parent.get_out_storage())))
    {
        ComPtr<IEnumShellItems> enum_shell_items;
        if (SUCCEEDED(parent->BindToHandler(nullptr, BHID_StorageEnum, IID_PPV_ARGS(enum_shell_items.get_out_storage()))))
        {
            IShellItem*	child[1000];
            ULONG count = ARRAYSIZE(child);
            ULONG fetched;
            while (SUCCEEDED(enum_shell_items->Next(count, child, &fetched)) && (fetched > 0))
            {
                for (ULONG i = 0; i < fetched; ++i) {
                    int order = 0;
                    if ((m_index == 0)
                        && SUCCEEDED(shellItem->Compare(child[i], SICHINT_TEST_FILESYSPATH_IF_NOT_EQUAL, &order))
                        && (order == 0))
                    {
                        m_index = m_files.size();
                    }
                    m_files.emplace_back(child[i]);
                }
            }
        }
    }
}

bool ShellNavigator::GetNext(IShellItem** shellItem)
{
    if (m_index < m_files.size() - 1)
    {
        ++m_index;
        *shellItem = m_files[m_index].new_ref();
        return true;
    }
    *shellItem = nullptr;
    return false;
}

bool ShellNavigator::GetPrevious(IShellItem** shellItem)
{
    if (m_index > 0)
    {
        --m_index;
        *shellItem = m_files[m_index].new_ref();
        return true;
    }
    *shellItem = nullptr;
    return false;
}
