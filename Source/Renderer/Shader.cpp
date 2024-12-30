// © 2022 Adam Badke. All rights reserved.
#include "Material.h"
#include "RenderManager.h"
#include "Shader.h"
#include "Shader_Platform.h"

#include "Core/Assert.h"
#include "Core/InvPtr.h"

#include "Core/Interfaces/ILoadContext.h"

#include "Core/Util/HashUtils.h"


namespace
{
	// We may reuse the same shader files, but with a different pipeline state. So here, we compute a unique identifier
	// to represent a particular configuration
	ShaderID ComputeShaderIdentifier(
		std::vector<std::pair<std::string, re::Shader::ShaderType>> const& extensionlessSourceFilenames,
		re::PipelineState const* rePipelineState)
	{
		ShaderID hashResult = 0;

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

		if (!isComputeShader && rePipelineState)
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
	[[nodiscard]] core::InvPtr<re::Shader> Shader::GetOrCreate(
		std::vector<std::pair<std::string, ShaderType>> const& extensionlessSourceFilenames,
		re::PipelineState const* rePipelineState,
		re::VertexStreamMap const* vertexStreamMap)
	{
		const ShaderID shaderID = ComputeShaderIdentifier(extensionlessSourceFilenames, rePipelineState);

		// If the shader already exists, return it. Otherwise, create the shader. 
		core::Inventory* inventory = re::RenderManager::Get()->GetInventory();
		if (inventory->HasLoaded<re::Shader>(shaderID))
		{
			return inventory->Get<re::Shader>(shaderID);
		}


		struct ShaderLoadContext : core::ILoadContext<re::Shader>
		{
			void OnLoadBegin(core::InvPtr<re::Shader> newShader) override
			{
				LOG(std::format("Scheduling load for Shader with ID \"{}\"", m_shaderID).c_str());

				// Register for API-layer creation now to ensure we don't miss our chance for the current frame
				re::RenderManager::Get()->RegisterForCreate(newShader);
			}

			std::unique_ptr<re::Shader> Load(core::InvPtr<re::Shader>) override
			{
				bool isComputeShader = false;

				// Concatenate the various filenames together to build a helpful identifier
				std::string shaderName;
				for (size_t i = 0; i < m_extensionlessSrcFilenames.size(); ++i)
				{
					std::string const& filename = m_extensionlessSrcFilenames.at(i).first;
					const ShaderType shaderType = m_extensionlessSrcFilenames.at(i).second;

					shaderName += std::format("{}={}{}",
						ShaderTypeToCStr(shaderType),
						filename,
						i == m_extensionlessSrcFilenames.size() - 1 ? "" : "__");

					if (shaderType == re::Shader::Compute)
					{
						isComputeShader = true;
					}
				}
				LOG(std::format("Loading Shader \"{}\" (ID {})", shaderName, m_shaderID).c_str());

				SEAssert(!isComputeShader || m_extensionlessSrcFilenames.size() == 1,
					"A compute shader should only have a single shader entry. This is unexpected");
				SEAssert(m_rePipelineState || isComputeShader,
					"PipelineState is null. This is unexpected for non-compute shaders");
				SEAssert(m_vertexStreamMap != nullptr || isComputeShader, "Invalid attempt to set a VertexStreamMap");

				return std::unique_ptr<re::Shader>(new re::Shader(
					shaderName, m_extensionlessSrcFilenames, m_rePipelineState, m_vertexStreamMap, m_shaderID));				
			}

			ShaderID m_shaderID;
			std::vector<std::pair<std::string, ShaderType>> m_extensionlessSrcFilenames;
			re::PipelineState const* m_rePipelineState;
			re::VertexStreamMap const* m_vertexStreamMap;
		};
		std::shared_ptr<ShaderLoadContext> shaderLoadContext = std::make_shared<ShaderLoadContext>();

		shaderLoadContext->m_shaderID = shaderID;
		shaderLoadContext->m_extensionlessSrcFilenames = extensionlessSourceFilenames;
		shaderLoadContext->m_rePipelineState = rePipelineState;
		shaderLoadContext->m_vertexStreamMap = vertexStreamMap;

		return inventory->Get(
			shaderID,
			static_pointer_cast<core::ILoadContext<re::Shader>>(shaderLoadContext));
	}


	Shader::Shader(
		std::string const& shaderName,
		std::vector<std::pair<std::string, ShaderType>> const& extensionlessSourceFilenames,
		re::PipelineState const* rePipelineState,
		re::VertexStreamMap const* m_vertexStreamMap,
		uint64_t shaderIdentifier)
		: INamedObject(shaderName)
		, m_shaderIdentifier(shaderIdentifier)
		, m_extensionlessSourceFilenames(extensionlessSourceFilenames)
		, m_pipelineState(rePipelineState)
		, m_vertexStreamMap(m_vertexStreamMap)
	{
		SEAssert(rePipelineState ||
		(m_extensionlessSourceFilenames.size() == 1 && m_extensionlessSourceFilenames[0].second == re::Shader::Compute),
			"re PipelineState is null. This is unexpected");

		platform::Shader::CreatePlatformParams(*this);
	}


	Shader::~Shader()
	{
		SEAssert(m_platformParams == nullptr, "Platform parameters is not null. Was Destroy() called?");
	}


	void Shader::Destroy()
	{
		platform::Shader::Destroy(*this);
		m_platformParams = nullptr;
	}
}
