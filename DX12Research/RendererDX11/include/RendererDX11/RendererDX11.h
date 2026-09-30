#pragma once

#include "Core/IRenderer.h"

#include <memory>
#include <string>

namespace RendererDX11
{
    std::unique_ptr<Core::IRenderer> CreateDX11Renderer();
    std::string DumpDX11Features(const std::wstring& strAdapterFilter);
}
