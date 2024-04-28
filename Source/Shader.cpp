// © 2022 Adam Badke. All rights reserved.
#include "Core\Assert.h"
#include "Core\Util\HashUtils.h"
#include "Material.h"
#include "RenderManager.h"
#include "SceneData.h"
#include "SceneManager.h"
#include "Shader.h"
#include "Shader_Platform.h"


namespace re
{
	uint64_t Shader::ComputeShaderIdentifier(
		std::string const& extensionlessShaderFilename, re::PipelineState const& rePipelineState)
	{
		uint64_t hashResult = util::HashString(extensionlessShaderFilename);
		util::CombineHash(hashResult, rePipelineState.GetDataHash());
		return hashResult;
	}


	std::shared_ptr<re::Shader> Shader::GetOrCreate(
		std::string const& extensionlessShaderFilename, re::PipelineState const& rePipelineState)
	{
		// We may reuse the same shader, but with a different pipeline state. So here, we compute a unique identifier
		// to represent this particular configuration
		const uint64_t shaderIdentifier = ComputeShaderIdentifier(extensionlessShaderFilename, rePipelineState);

		// If the shader already exists, return it. Otherwise, create the shader. 
		fr::SceneData* sceneData = fr::SceneManager::GetSceneData();
		if (sceneData->ShaderExists(shaderIdentifier))
		{
			return sceneData->GetShader(shaderIdentifier);
		}
		// Note: It's possible that 2 threads might simultaneously fail to find a Shader in the SceneData, and create
		// it. But that's OK, the SceneData will tell us if this shader was actually added

		// Our ctor is private; We must manually create the Shader, and then pass the ownership to a shared_ptr
		std::shared_ptr<re::Shader> sharedShaderPtr;
		sharedShaderPtr.reset(new re::Shader(extensionlessShaderFilename, rePipelineState, shaderIdentifier));

		// Register the Shader with the SceneData object for lifetime management:
		const bool addedNewShader = sceneData->AddUniqueShader(sharedShaderPtr);
		if (addedNewShader)
		{
			// Register the Shader with the RenderManager (once only), so its API-level object can be created before use
			re::RenderManager::Get()->RegisterForCreate(sharedShaderPtr);
		}

		return sharedShaderPtr;
	}


	Shader::Shader(
		std::string const& extensionlessShaderFilename, re::PipelineState const& rePipelineState, uint64_t shaderIdentifier)
		: INamedObject(extensionlessShaderFilename)
		, m_shaderIdentifier(shaderIdentifier)
		, m_pipelineState(rePipelineState)
	{
		platform::Shader::CreatePlatformParams(*this);
	}


	void Shader::Destroy()
	{
		platform::Shader::Destroy(*this);
	}
}
