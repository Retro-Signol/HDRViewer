#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <algorithm>

class SlideShow {
    std::vector<std::wstring> m_files;
    int m_index = -1;
    bool m_loop = true;

public:
    void LoadDirectory(const std::wstring& imagePath) {
        std::wstring dir = imagePath;
        size_t pos = dir.rfind(L'\\');
        if (pos == std::wstring::npos) return;
        dir = dir.substr(0, pos);

        const wchar_t* exts[] = {
            L"*.jxl", L"*.exr", L"*.jxr", L"*.wdp", L"*.hdp",
            L"*.png", L"*.jpg", L"*.jpeg", L"*.bmp", L"*.tga", L"*.gif"
        };

        m_files.clear();
        for (auto* ext : exts) {
            WIN32_FIND_DATAW fd;
            std::wstring pattern = dir + L"\\" + ext;
            HANDLE hFind = FindFirstFileW(pattern.c_str(), &fd);
            if (hFind == INVALID_HANDLE_VALUE) continue;
            do {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                    m_files.push_back(dir + L"\\" + fd.cFileName);
            } while (FindNextFileW(hFind, &fd));
            FindClose(hFind);
        }

        std::sort(m_files.begin(), m_files.end(),
            [](const auto& a, const auto& b) { return _wcsicmp(a.c_str(), b.c_str()) < 0; });

        for (size_t i = 0; i < m_files.size(); i++) {
            if (_wcsicmp(m_files[i].c_str(), imagePath.c_str()) == 0) {
                m_index = static_cast<int>(i);
                break;
            }
        }
        if (m_index < 0) m_index = 0;
    }

    std::wstring Prev() {
        if (m_files.empty()) return L"";
        if (m_index <= 0) m_index = m_loop ? static_cast<int>(m_files.size()) - 1 : 0;
        else m_index--;
        return m_files[m_index];
    }

    std::wstring Next() {
        if (m_files.empty()) return L"";
        if (m_index + 1 >= static_cast<int>(m_files.size()))
            m_index = m_loop ? 0 : m_index;
        else m_index++;
        return m_files[m_index];
    }

    int Count() const { return static_cast<int>(m_files.size()); }
    int Index() const { return m_index; }
};
