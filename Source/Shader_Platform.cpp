// © 2022 Adam Badke. All rights reserved.
#include "Assert.h"
#include "RenderManager.h"
#include "RootSignature_DX12.h"
#include "Shader_DX12.h"
#include "Shader_OpenGL.h"
#include "Shader_Platform.h"


namespace platform
{
	void platform::Shader::CreatePlatformParams(re::Shader& shader)
	{
		const platform::RenderingAPI api = re::RenderManager::Get()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			shader.SetPlatformParams(std::make_unique<opengl::Shader::PlatformParams>());
		}
		break;
		case RenderingAPI::DX12:
		{
			shader.SetPlatformParams(std::make_unique<dx12::Shader::PlatformParams>());
		}
		break;
		default:
		{
			SEAssertF("Invalid rendering API argument received");
		}
		}
	}


	void (*platform::Shader::Create)(re::Shader&) = nullptr;
	void (*platform::Shader::Destroy)(re::Shader&) = nullptr;
}