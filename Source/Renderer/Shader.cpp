// © 2022 Adam Badke. All rights reserved.
#include "Material.h"
#include "RenderManager.h"
#include "Shader.h"
#include "Shader_Platform.h"

#include "Core/Assert.h"
#include "Core/InvPtr.h"
#include "Core/Logger.h"

#include "Core/Interfaces/ILoadContext.h"

#include "Core/Util/HashUtils.h"


namespace
{
	// We may reuse the same shader files, but with a different pipeline state. So here, we compute a unique identifier
	// to represent a particular configuration
	ShaderID ComputeShaderIdentifier(
		std::vector<std::pair<std::string, re::Shader::ShaderType>> const& extensionlessSourceFilenames,
		re::RasterizationState const* rasterizationState)
	{
		SEAssert(!extensionlessSourceFilenames.empty(), "Shader source filenames is empty");

		ShaderID hashResult = 0;

		const re::Shader::ShaderType firstShaderType = extensionlessSourceFilenames[0].second;
		for (auto const& shaderStage : extensionlessSourceFilenames)
		{
			const re::Shader::ShaderType shaderType = shaderStage.second;
			SEAssert(re::Shader::IsSamePipelineType(shaderType, firstShaderType),
				"Found shaders with mixed pipeline types");

			util::CombineHash(hashResult, util::HashString(shaderStage.first));
			util::CombineHash(hashResult, shaderStage.second);
		}

		if (re::Shader::IsRasterizationType(firstShaderType))
		{
			SEAssert(rasterizationState, "Pipeline state is null. This is unexpected for rasterization pipelines");

			util::CombineHash(hashResult, rasterizationState->GetDataHash());
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


	re::Shader::PipelineType FindPipelineType(
		std::vector<std::pair<std::string, re::Shader::ShaderType>> const& extensionlessSourceFilenames)
	{
		SEAssert(!extensionlessSourceFilenames.empty(), "No source files to evaluate");

		re::Shader::ShaderType firstType = re::Shader::ShaderType::ShaderType_Count;
		for (auto const& source : extensionlessSourceFilenames)
		{
			if (!source.first.empty())
			{
				firstType = source.second;
				break;
			}
		}

		switch (firstType)
		{
		case re::Shader::ShaderType::Vertex:
		case re::Shader::ShaderType::Geometry:
		case re::Shader::ShaderType::Pixel:
		case re::Shader::ShaderType::Hull:
		case re::Shader::ShaderType::Domain:
			return re::Shader::PipelineType::Rasterization;
		case re::Shader::ShaderType::Amplification:
		case re::Shader::ShaderType::Mesh:
			return re::Shader::PipelineType::Mesh;
		case re::Shader::ShaderType::Compute:
			return re::Shader::PipelineType::Compute;
		case re::Shader::ShaderType::HitGroup_Intersection:
		case re::Shader::ShaderType::HitGroup_AnyHit:
		case re::Shader::ShaderType::HitGroup_ClosestHit:
		case re::Shader::ShaderType::Callable:
		case re::Shader::ShaderType::RayGen:
		case re::Shader::ShaderType::Miss:
			return re::Shader::PipelineType::RayTracing;
		default: SEAssertF("Invalid type");
		}
	}
}

namespace re
{
	[[nodiscard]] core::InvPtr<re::Shader> Shader::GetOrCreate(
		std::vector<std::pair<std::string, ShaderType>> const& extensionlessSourceFilenames,
		re::RasterizationState const* rasterizationState,
		re::VertexStreamMap const* vertexStreamMap)
	{
		const ShaderID shaderID = ComputeShaderIdentifier(extensionlessSourceFilenames, rasterizationState);

		// If the shader already exists, return it. Otherwise, create the shader. 
		core::Inventory* inventory = re::RenderManager::Get()->GetInventory();
		if (inventory->Has<re::Shader>(shaderID))
		{
			return inventory->Get<re::Shader>(shaderID);
		}


		struct ShaderLoadContext : core::ILoadContext<re::Shader>
		{
			void OnLoadBegin(core::InvPtr<re::Shader>& newShader) override
			{
				LOG(std::format("Scheduling load for Shader with ID \"{}\"", m_shaderID).c_str());

				// Register for API-layer creation now to ensure we don't miss our chance for the current frame
				re::RenderManager::Get()->RegisterForCreate(newShader);
			}

			std::unique_ptr<re::Shader> Load(core::InvPtr<re::Shader>&) override
			{
				SEAssert(!m_extensionlessSrcFilenames.empty(), "Shader source filenames is empty");

				// Concatenate the various filenames together to build a helpful identifier
				std::string shaderName;
				const re::Shader::ShaderType firstShaderType = m_extensionlessSrcFilenames[0].second;
				for (size_t i = 0; i < m_extensionlessSrcFilenames.size(); ++i)
				{
					std::string const& filename = m_extensionlessSrcFilenames.at(i).first;
					const ShaderType shaderType = m_extensionlessSrcFilenames.at(i).second;

					shaderName += std::format("{}={}{}",
						ShaderTypeToCStr(shaderType),
						filename,
						i == m_extensionlessSrcFilenames.size() - 1 ? "" : "__");
				}
				LOG(std::format("Loading Shader \"{}\" (ID {})", shaderName, m_shaderID).c_str());

				SEAssert(!re::Shader::IsComputeType(firstShaderType) || m_extensionlessSrcFilenames.size() == 1,
					"A compute shader should only have a single shader entry. This is unexpected");
				SEAssert(m_rasterizationState || !re::Shader::IsRasterizationType(firstShaderType),
					"RasterizationState is null. This is unexpected for rasterization pipelines");
				SEAssert(m_vertexStreamMap || !re::Shader::IsRasterizationType(firstShaderType),
					"VertexStreamMap is null. This is unexpected for rasterization pipelines");

				return std::unique_ptr<re::Shader>(new re::Shader(
					shaderName, m_extensionlessSrcFilenames, m_rasterizationState, m_vertexStreamMap, m_shaderID));				
			}

			ShaderID m_shaderID;
			std::vector<std::pair<std::string, ShaderType>> m_extensionlessSrcFilenames;
			re::RasterizationState const* m_rasterizationState;
			re::VertexStreamMap const* m_vertexStreamMap;
		};
		std::shared_ptr<ShaderLoadContext> shaderLoadContext = std::make_shared<ShaderLoadContext>();

		shaderLoadContext->m_shaderID = shaderID;
		shaderLoadContext->m_extensionlessSrcFilenames = extensionlessSourceFilenames;
		shaderLoadContext->m_rasterizationState = rasterizationState;
		shaderLoadContext->m_vertexStreamMap = vertexStreamMap;

		return inventory->Get(
			shaderID,
			static_pointer_cast<core::ILoadContext<re::Shader>>(shaderLoadContext));
	}


	Shader::Shader(
		std::string const& shaderName,
		std::vector<std::pair<std::string, ShaderType>> const& extensionlessSourceFilenames,
		re::RasterizationState const* rasterizationState,
		re::VertexStreamMap const* m_vertexStreamMap,
		uint64_t shaderIdentifier)
		: INamedObject(shaderName)
		, m_shaderIdentifier(shaderIdentifier)
		, m_extensionlessSourceFilenames(extensionlessSourceFilenames)
		, m_pipelineType(FindPipelineType(extensionlessSourceFilenames))
		, m_rasterizationState(rasterizationState)
		, m_vertexStreamMap(m_vertexStreamMap)
	{
		SEAssert(rasterizationState || !IsRasterizationType(m_extensionlessSourceFilenames[0].second),
			"RasterizationState is null. This is unexpected for rasterization pipelines");

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
