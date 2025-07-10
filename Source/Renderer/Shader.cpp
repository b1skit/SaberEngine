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
		std::vector<re::Shader::Metadata> const& metadata,
		re::RasterizationState const* rasterizationState)
	{
		SEAssert(!metadata.empty(), "Shader source filenames is empty");

		ShaderID hashResult = 0;

		const re::Shader::ShaderType firstShaderType = metadata[0].m_type;
		for (auto const& shaderStage : metadata)
		{
			SEAssert(re::Shader::IsSamePipelineType(shaderStage.m_type, firstShaderType),
				"Found shaders with mixed pipeline types");

			util::CombineHash(hashResult, util::HashString(shaderStage.m_extensionlessFilename));
			util::CombineHash(hashResult, util::HashString(shaderStage.m_entryPoint));
			util::CombineHash(hashResult, shaderStage.m_type);
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


	re::Shader::PipelineType FindPipelineType(std::vector<re::Shader::Metadata> const& metadata)
	{
		SEAssert(!metadata.empty(), "No source files to evaluate");

		re::Shader::ShaderType firstType = re::Shader::ShaderType::ShaderType_Count;
		for (auto const& source : metadata)
		{
			if (!source.m_extensionlessFilename.empty())
			{
				firstType = source.m_type;
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
		return re::Shader::PipelineType::Rasterization; // This should never happen
	}
}

namespace re
{
	[[nodiscard]] core::InvPtr<re::Shader> Shader::GetOrCreate(
		std::vector<Metadata> const& metadata,
		re::RasterizationState const* rasterizationState,
		re::VertexStreamMap const* vertexStreamMap)
	{
		const ShaderID shaderID = ComputeShaderIdentifier(metadata, rasterizationState);

		// If the shader already exists, return it. Otherwise, create the shader. 
		core::Inventory* inventory = gr::RenderManager::Get()->GetInventory();
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
				gr::RenderManager::Get()->GetContext()->RegisterForCreate(newShader);
			}

			std::unique_ptr<re::Shader> Load(core::InvPtr<re::Shader>&) override
			{
				SEAssert(!m_metadata.empty(), "Shader metadata is empty");

				// Concatenate the various filenames together to build a helpful identifier
				std::string shaderName;
				const re::Shader::ShaderType firstShaderType = m_metadata[0].m_type;
				for (size_t i = 0; i < m_metadata.size(); ++i)
				{
					std::string const& filename = m_metadata.at(i).m_extensionlessFilename;
					const ShaderType shaderType = m_metadata.at(i).m_type;

					shaderName += std::format("{}={}{}",
						ShaderTypeToCStr(shaderType),
						filename,
						i == m_metadata.size() - 1 ? "" : "__");
				}
				LOG(std::format("Loading Shader \"{}\" (ID {})", shaderName, m_shaderID).c_str());

				SEAssert(!re::Shader::IsComputeType(firstShaderType) || m_metadata.size() == 1,
					"A compute shader should only have a single shader entry. This is unexpected");
				SEAssert(m_rasterizationState || !re::Shader::IsRasterizationType(firstShaderType),
					"RasterizationState is null. This is unexpected for rasterization pipelines");
				SEAssert(m_vertexStreamMap || !re::Shader::IsRasterizationType(firstShaderType),
					"VertexStreamMap is null. This is unexpected for rasterization pipelines");

				return std::unique_ptr<re::Shader>(new re::Shader(
					shaderName, m_metadata, m_rasterizationState, m_vertexStreamMap, m_shaderID));
			}

			ShaderID m_shaderID;
			std::vector<Metadata> m_metadata;
			re::RasterizationState const* m_rasterizationState;
			re::VertexStreamMap const* m_vertexStreamMap;
		};
		std::shared_ptr<ShaderLoadContext> shaderLoadContext = std::make_shared<ShaderLoadContext>();

		shaderLoadContext->m_shaderID = shaderID;
		shaderLoadContext->m_metadata = metadata;
		shaderLoadContext->m_rasterizationState = rasterizationState;
		shaderLoadContext->m_vertexStreamMap = vertexStreamMap;

		return inventory->Get(
			shaderID,
			static_pointer_cast<core::ILoadContext<re::Shader>>(shaderLoadContext));
	}


	Shader::Shader(
		std::string const& shaderName,
		std::vector<Metadata> const& metadata,
		re::RasterizationState const* rasterizationState,
		re::VertexStreamMap const* m_vertexStreamMap,
		ShaderID shaderIdentifier)
		: INamedObject(shaderName)
		, m_shaderIdentifier(shaderIdentifier)
		, m_metadata(metadata)
		, m_pipelineType(FindPipelineType(metadata))
		, m_rasterizationState(rasterizationState)
		, m_vertexStreamMap(m_vertexStreamMap)
	{
		SEAssert(rasterizationState || !IsRasterizationType(m_metadata[0].m_type),
			"RasterizationState is null. This is unexpected for rasterization pipelines");

		platform::Shader::CreatePlatformObject(*this);
	}


	Shader::~Shader()
	{
		SEAssert(m_platObj == nullptr, "Platform parameters is not null. Was Destroy() called?");
	}


	void Shader::Destroy()
	{
		platform::Shader::Destroy(*this);
		m_platObj = nullptr;
	}
}
