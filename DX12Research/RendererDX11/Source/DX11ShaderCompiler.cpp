#include "DX11ShaderCompiler.h"

#include "Core/HrCheck.h"
#include "Core/Log.h"

#include <d3dcompiler.h>

#include <filesystem>
#include <format>
#include <vector>

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "version.lib")

namespace RendererDX11
{
    namespace
    {
        constexpr const wchar_t kSetupTestFile[] = L"SetupTest.hlsl";
        constexpr const wchar_t kFxcModule[] = L"d3dcompiler_47.dll";
        constexpr const char kVsEntry[] = "VSMain";
        constexpr const char kPsEntry[] = "PSMain";
        constexpr const char kVsTarget[] = "vs_5_0";
        constexpr const char kPsTarget[] = "ps_5_0";
#if defined(_DEBUG)
        constexpr UINT kCompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        constexpr UINT kCompileFlags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

        std::string BlobText(ID3DBlob* pBlob)
        {
            if (nullptr == pBlob)
            {
                return std::string();
            }
            std::string strText(static_cast<const char*>(pBlob->GetBufferPointer()), pBlob->GetBufferSize());
            while (false == strText.empty() && ('\0' == strText.back() || '\n' == strText.back() || '\r' == strText.back()))
            {
                strText.pop_back();
            }
            return strText;
        }

        std::string FileVersionText(const std::wstring& strPath)
        {
            DWORD dwHandle = 0;
            const DWORD dwSize = GetFileVersionInfoSizeW(strPath.c_str(), &dwHandle);
            if (0 == dwSize)
            {
                return "unknown";
            }
            std::vector<BYTE> vecData(dwSize);
            if (FALSE == GetFileVersionInfoW(strPath.c_str(), 0, dwSize, vecData.data()))
            {
                return "unknown";
            }
            VS_FIXEDFILEINFO* pInfo = nullptr;
            UINT uLength = 0;
            if (FALSE == VerQueryValueW(vecData.data(), L"\\", reinterpret_cast<LPVOID*>(&pInfo), &uLength) || nullptr == pInfo)
            {
                return "unknown";
            }
            return std::format("{}.{}.{}.{}", HIWORD(pInfo->dwFileVersionMS), LOWORD(pInfo->dwFileVersionMS), HIWORD(pInfo->dwFileVersionLS), LOWORD(pInfo->dwFileVersionLS));
        }

        void CompileAndLog(const std::filesystem::path& pathFile, const char* szEntryPoint, const char* szTarget)
        {
            ShaderCompileResult result;
            const std::string strFile = Core::ToUtf8(pathFile.filename().wstring());
            if (true == CompileShaderFxc(pathFile.wstring(), szEntryPoint, szTarget, result))
            {
                Core::Log::Info("shader {} {} {} ok {}", strFile, szEntryPoint, szTarget, result.m_pBytecode->GetBufferSize());
                if (false == result.m_strMessages.empty())
                {
                    Core::Log::Warn("shader {} {} {} {}", strFile, szEntryPoint, szTarget, result.m_strMessages);
                }
                return;
            }
            Core::Log::Warn("shader {} {} {} failed {}", strFile, szEntryPoint, szTarget, result.m_strMessages);
        }
    }

    bool CompileShaderFxc(const std::wstring& strFilePath, const char* szEntryPoint, const char* szTarget, ShaderCompileResult& result)
    {
        Microsoft::WRL::ComPtr<ID3DBlob> pErrors;
        const HRESULT hrCompile = D3DCompileFromFile(strFilePath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, szEntryPoint, szTarget, kCompileFlags, 0, &result.m_pBytecode, &pErrors);
        result.m_strMessages = BlobText(pErrors.Get());
        if (FAILED(hrCompile))
        {
            if (true == result.m_strMessages.empty())
            {
                result.m_strMessages = Core::HrToString(hrCompile);
            }
            result.m_pBytecode.Reset();
            return false;
        }
        return true;
    }

    std::string QueryFxcVersion()
    {
        HMODULE hModule = GetModuleHandleW(kFxcModule);
        if (nullptr == hModule)
        {
            hModule = LoadLibraryW(kFxcModule);
        }
        if (nullptr == hModule)
        {
            return "not loaded";
        }
        wchar_t szPath[MAX_PATH] = {};
        if (0 == GetModuleFileNameW(hModule, szPath, MAX_PATH))
        {
            return "unknown";
        }
        return std::format("{} {}", FileVersionText(szPath), Core::ToUtf8(szPath));
    }

    void LogSetupTestCompile(const std::wstring& strShaderDir)
    {
        const std::filesystem::path pathFile = std::filesystem::path(strShaderDir) / kSetupTestFile;
        Core::Log::Info("fxc {}", QueryFxcVersion());
        CompileAndLog(pathFile, kVsEntry, kVsTarget);
        CompileAndLog(pathFile, kPsEntry, kPsTarget);
    }
}
