// © 2022 Adam Badke. All rights reserved.
#include "DebugConfiguration.h"
#include "Material.h"
#include "RenderManager.h"
#include "SceneData.h"
#include "SceneManager.h"
#include "Shader.h"
#include "Shader_Platform.h"

using std::string;
using std::vector;
using std::shared_ptr;
using std::make_shared;
using gr::Material;


namespace re
{
	std::shared_ptr<re::Shader> Shader::Create(std::string const& extensionlessShaderFilename)
	{
		// If the shader already exists, return it. Otherwise, create the shader. 
		if (en::SceneManager::GetSceneData()->ShaderExists(extensionlessShaderFilename))
		{
			return en::SceneManager::GetSceneData()->GetShader(extensionlessShaderFilename);
		}
		// Note: It's possible that 2 threads might simultaneously fail to find a Shader in the SceneData, and create
		// it. But that's OK, the SceneData will only allow 1 instance to be added

		// Our ctor is private; We must manually create the Shader, and then pass the ownership to a shared_ptr
		shared_ptr<re::Shader> sharedShaderPtr;
		sharedShaderPtr.reset(new re::Shader(extensionlessShaderFilename));

		// Register the Shader with the SceneData object for lifetime management:
		en::SceneManager::GetSceneData()->AddUniqueShader(sharedShaderPtr);

		// Register the Shader with the RenderManager, so its API-level object can be created before use
		re::RenderManager::Get()->RegisterShaderForCreate(sharedShaderPtr);

		return sharedShaderPtr;
	}


	Shader::Shader(string const& extensionlessShaderFilename)
		: NamedObject(extensionlessShaderFilename)
	{
		platform::Shader::CreatePlatformParams(*this);
	}


	void Shader::Destroy()
	{
		platform::Shader::Destroy(*this);
	}
}
