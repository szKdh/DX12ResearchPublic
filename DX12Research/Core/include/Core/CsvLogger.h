#pragma once

#include <string>
#include <vector>

namespace Core
{
    class CsvLogger
    {
    public:
        bool Open(const std::wstring& strFilePath, const std::vector<std::string>& vecHeader);
        void AppendRow(const std::vector<std::string>& vecValues);
        void Close();
    };
}
