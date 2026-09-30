#pragma once

#include <d3dcommon.h>
#include <wrl/client.h>

#include <string>

namespace RendererDX11
{
    struct ShaderCompileResult
    {
        Microsoft::WRL::ComPtr<ID3DBlob> m_pBytecode;
        std::string m_strMessages;
    };

    bool CompileShaderFxc(const std::wstring& strFilePath, const char* szEntryPoint, const char* szTarget, ShaderCompileResult& result);
    std::string QueryFxcVersion();
    void LogSetupTestCompile(const std::wstring& strShaderDir);
}
