// © 2022 Adam Badke. All rights reserved.
#include "Core/Assert.h"
#include "Private/RenderManager.h"
#include "Private/RootSignature_DX12.h"
#include "Private/Shader_DX12.h"
#include "Private/Shader_OpenGL.h"
#include "Private/Shader_Platform.h"


namespace platform
{
	void platform::Shader::CreatePlatformObject(re::Shader& shader)
	{
		const platform::RenderingAPI api = re::RenderManager::Get()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			shader.SetPlatformObject(std::make_unique<opengl::Shader::PlatObj>());
		}
		break;
		case RenderingAPI::DX12:
		{
			shader.SetPlatformObject(std::make_unique<dx12::Shader::PlatObj>());
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