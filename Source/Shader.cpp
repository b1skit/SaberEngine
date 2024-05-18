// © 2022 Adam Badke. All rights reserved.
#include "Material.h"
#include "RenderManager.h"
#include "SceneData.h"
#include "SceneManager.h"
#include "Shader.h"
#include "Shader_Platform.h"

#include "Core\Assert.h"

#include "Core\Util\HashUtils.h"


namespace
{
	// We may reuse the same shader files, but with a different pipeline state. So here, we compute a unique identifier
	// to represent a particular configuration
	ShaderID ComputeShaderIdentifier(
		std::vector<std::pair<std::string, re::Shader::ShaderType>> const& extensionlessSourceFilenames,
		re::PipelineState const* rePipelineState)
	{
		uint64_t hashResult = 0;

		for (auto const& shaderStage : extensionlessSourceFilenames)
		{
			SEAssert(shaderStage.second != re::Shader::ShaderType_Count, "Invalid shader type");

			util::CombineHash(hashResult, util::HashString(shaderStage.first));
			util::CombineHash(hashResult, shaderStage.second);
		}

		util::CombineHash(hashResult, rePipelineState->GetDataHash());
		
		return hashResult;
	}
}

namespace re
{
	[[nodiscard]] std::shared_ptr<re::Shader> Shader::GetOrCreate(
		std::vector<std::pair<std::string, ShaderType>> const& extensionlessSourceFilenames,
		re::PipelineState const* rePipelineState)
	{
		SEAssert(rePipelineState, "PipelineState is null. This is (currently) unexpected");

		const ShaderID shaderID = ComputeShaderIdentifier(extensionlessSourceFilenames, rePipelineState);

		// Concatenate the various filenames together to build a helpful identifier
		std::string shaderName;
		for (size_t i = 0; i < extensionlessSourceFilenames.size(); ++i)
		{
			std::string const& filename = extensionlessSourceFilenames.at(i).first;
			ShaderType shaderType = extensionlessSourceFilenames.at(i).second;
			shaderName += std::format("{}:{}{}",
				k_shaderTypeNames[shaderType],
				filename,
				i == extensionlessSourceFilenames.size() - 1 ? "" : "|");
		}

		// If the shader already exists, return it. Otherwise, create the shader. 
		fr::SceneData* sceneData = fr::SceneManager::GetSceneData();
		if (sceneData->ShaderExists(shaderID))
		{
			return sceneData->GetShader(shaderID);
		}
		// Note: It's possible that 2 threads might simultaneously fail to find a Shader in the SceneData, and create
		// it. But that's OK, the SceneData will tell us if this shader was actually added

		// Our ctor is private; We must manually create the Shader, and then pass the ownership to a shared_ptr
		std::shared_ptr<re::Shader> sharedShaderPtr;
		sharedShaderPtr.reset(new re::Shader(shaderName, extensionlessSourceFilenames, rePipelineState, shaderID));

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
		std::string const& shaderName,
		std::vector<std::pair<std::string, ShaderType>> const& extensionlessSourceFilenames,
		re::PipelineState const* rePipelineState,
		uint64_t shaderIdentifier)
		: INamedObject(shaderName)
		, m_shaderIdentifier(shaderIdentifier)
		, m_extensionlessSourceFilenames(extensionlessSourceFilenames)
		, m_pipelineState(rePipelineState)
	{
		platform::Shader::CreatePlatformParams(*this);
	}


	Shader::~Shader()
	{
		platform::Shader::Destroy(*this);
	}
}
