// © 2022 Adam Badke. All rights reserved.
#include "RenderManager.h"
#include "Shader_DX12.h"
#include "Shader_OpenGL.h"
#include "Shader_Platform.h"

#include "Core/Assert.h"


namespace platform
{
	void platform::Shader::CreatePlatformObject(re::Shader& shader)
	{
		const platform::RenderingAPI api = gr::RenderManager::Get()->GetRenderingAPI();

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