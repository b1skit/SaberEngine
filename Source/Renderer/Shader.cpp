// © 2022 Adam Badke. All rights reserved.
#include "Material.h"
#include "RenderManager.h"
#include "SceneData.h"
#include "Shader.h"
#include "Shader_Platform.h"

#include "Core/Assert.h"

#include "Core/Util/HashUtils.h"


namespace
{
	// We may reuse the same shader files, but with a different pipeline state. So here, we compute a unique identifier
	// to represent a particular configuration
	ShaderID ComputeShaderIdentifier(
		std::vector<std::pair<std::string, re::Shader::ShaderType>> const& extensionlessSourceFilenames,
		re::PipelineState const* rePipelineState)
	{
		uint64_t hashResult = 0;

		bool isComputeShader = false;
		for (auto const& shaderStage : extensionlessSourceFilenames)
		{
			const re::Shader::ShaderType shaderType = shaderStage.second;
			SEAssert(shaderType != re::Shader::ShaderType_Count, "Invalid shader type");

			if (shaderType == re::Shader::Compute)
			{
				isComputeShader = true;
			}

			util::CombineHash(hashResult, util::HashString(shaderStage.first));
			util::CombineHash(hashResult, shaderStage.second);
		}
		SEAssert(!isComputeShader || extensionlessSourceFilenames.size() == 1,
			"A compute shader should only have a single source file entry");

		SEAssert(rePipelineState || isComputeShader, "Pipeline state is null. This is unexpected");

		if (!isComputeShader)
		{
			util::CombineHash(hashResult, rePipelineState->GetDataHash());
		}
		
		return hashResult;
	}


	constexpr char const* ShaderTypeToCStr(re::Shader::ShaderType shaderType)
	{
		switch(shaderType)
		{
		case re::Shader::ShaderType::Vertex: return "Vertex";
		case re::Shader::ShaderType::Geometry: return "Geometry";
		case re::Shader::ShaderType::Pixel: return "Pixel";
		case re::Shader::ShaderType::Hull: return "Hull";
		case re::Shader::ShaderType::Domain: return "Domain";
		case re::Shader::ShaderType::Mesh: return "Mesh";
		case re::Shader::ShaderType::Amplification: return "Amplification";
		case re::Shader::ShaderType::Compute: return "Compute";
		default: SEAssertF("Invalid shader type");
		}
		return "INVALID_SHADER_TYPE"; // This should never happen
	}
}

namespace re
{
	[[nodiscard]] std::shared_ptr<re::Shader> Shader::GetOrCreate(
		std::vector<std::pair<std::string, ShaderType>> const& extensionlessSourceFilenames,
		re::PipelineState const* rePipelineState,
		re::VertexStreamMap const* vertexStreamMap)
	{
		const ShaderID shaderID = ComputeShaderIdentifier(extensionlessSourceFilenames, rePipelineState);

		// If the shader already exists, return it. Otherwise, create the shader. 
		re::SceneData* sceneData = re::RenderManager::GetSceneData();
		if (sceneData->ShaderExists(shaderID))
		{
			return sceneData->GetShader(shaderID);
		}
		// Note: It's possible that 2 threads might simultaneously fail to find a Shader in the SceneData, and create
		// it. But that's OK, the SceneData will tell us if this shader was actually added

		bool isComputeShader = false;

		// Concatenate the various filenames together to build a helpful identifier
		std::string shaderName;
		for (size_t i = 0; i < extensionlessSourceFilenames.size(); ++i)
		{
			std::string const& filename = extensionlessSourceFilenames.at(i).first;
			const ShaderType shaderType = extensionlessSourceFilenames.at(i).second;

			shaderName += std::format("{}={}{}",
				ShaderTypeToCStr(shaderType),
				filename,
				i == extensionlessSourceFilenames.size() - 1 ? "" : "__");

			if (shaderType == re::Shader::Compute)
			{
				isComputeShader = true;
			}
		}
		SEAssert(!isComputeShader || extensionlessSourceFilenames.size() == 1,
			"A compute shader should only have a single shader entry. This is unexpected");
		SEAssert(rePipelineState || isComputeShader, "PipelineState is null. This is unexpected for non-compute shaders");

		// Our ctor is private; We must manually create the Shader, and then pass the ownership to a shared_ptr
		std::shared_ptr<re::Shader> sharedShaderPtr;
		sharedShaderPtr.reset(new re::Shader(shaderName, extensionlessSourceFilenames, rePipelineState, shaderID));

		// Register the Shader with the SceneData object for lifetime management:
		const bool addedNewShader = sceneData->AddUniqueShader(sharedShaderPtr);
		if (addedNewShader)
		{
			// Register the Shader with the RenderManager (once only), so its API-level object can be created before use
			re::RenderManager::Get()->RegisterForCreateDEPRECATED(sharedShaderPtr);
		}

		SEAssert(vertexStreamMap != nullptr || isComputeShader,
			"Invalid attempt to set a VertexStreamMap");
		sharedShaderPtr->m_vertexStreamMap = vertexStreamMap;

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
		, m_vertexStreamMap(nullptr)
	{
		SEAssert(rePipelineState ||
		(m_extensionlessSourceFilenames.size() == 1 && m_extensionlessSourceFilenames[0].second == re::Shader::Compute),
			"re PipelineState is null. This is unexpected");

		platform::Shader::CreatePlatformParams(*this);
	}


	Shader::~Shader()
	{
		platform::Shader::Destroy(*this);
	}
}
