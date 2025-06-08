// © 2025 Adam Badke. All rights reserved.
#include "Private/Batch.h"
#include "Private/EffectDB.h"
#include "Private/GraphicsSystem_RayTracing_Experimental.h"
#include "Private/GraphicsSystemManager.h"
#include "Private/IndexedBuffer.h"
#include "Private/Material.h"
#include "Private/RenderManager.h"
#include "Private/RenderObjectIDs.h"
#include "Private/ShaderBindingTable.h"

#include "Core/Config.h"

#include "Core/Util/ImGuiUtils.h"

#include "Private/Renderer/Shaders/Common/RayTracingParams.h"
#include "Private/Renderer/Shaders/Common/ResourceCommon.h"
#include "Private/Renderer/Shaders/Common/TransformParams.h"


namespace
{
	std::shared_ptr<re::Buffer> CreateTraceRayParams(
		uint8_t instanceInclusionMask, RayFlag rayFlags, uint32_t missShaderIdx)
	{
		SEAssert(instanceInclusionMask <= 0xFF, "Instance inclusion mask has maximum 8 bits");

		const TraceRayData traceRayData{
			.g_traceRayParams = glm::uvec4(
				static_cast<uint32_t>(instanceInclusionMask),	// InstanceInclusionMask
				0,												// RayContributionToHitGroupIndex
				0,												// MultiplierForGeometryContributionToHitGroupIndex
				missShaderIdx),									// MissShaderIndex
			.g_rayFlags = glm::uvec4(
				rayFlags,
				0,
				0,
				0),
		};

		const re::Buffer::BufferParams traceRayBufferParams{
			.m_lifetime = re::Lifetime::SingleFrame,
			.m_stagingPool = re::Buffer::StagingPool::Temporary,
			.m_memPoolPreference = re::Buffer::MemoryPoolPreference::UploadHeap,
			.m_accessMask = re::Buffer::Access::GPURead | re::Buffer::Access::CPUWrite,
			.m_usageMask = re::Buffer::Usage::Constant,
		};

		return re::Buffer::Create("Trace Ray Params", traceRayData, traceRayBufferParams);
	}


	std::shared_ptr<re::Buffer> CreateDescriptorIndexesBuffer(
		ResourceHandle vertexStreamLUTsDescriptorIdx, 
		ResourceHandle instancedBufferLUTsDescriptorIdx,
		ResourceHandle cameraParamsDescriptorIdx,
		ResourceHandle targetUAVDescriptorIdx)
	{
		SEAssert(vertexStreamLUTsDescriptorIdx != INVALID_RESOURCE_IDX &&
			instancedBufferLUTsDescriptorIdx != INVALID_RESOURCE_IDX &&
			cameraParamsDescriptorIdx != INVALID_RESOURCE_IDX &&
			targetUAVDescriptorIdx != INVALID_RESOURCE_IDX,
			"Descriptor index is invalid. This is unexpected");

		// .x = VertexStreamLUTs, .y = InstancedBufferLUTs, .z = CameraParams, .w = output Texture2DRWFloat4 idx
		const DescriptorIndexData descriptorIndexData{
			.g_descriptorIndexes = glm::uvec4(
				vertexStreamLUTsDescriptorIdx,		// VertexStreamLUTs[]
				instancedBufferLUTsDescriptorIdx,	// InstancedBufferLUTs[]
				cameraParamsDescriptorIdx,			// CameraParams[]
				targetUAVDescriptorIdx),			// Texture2DRWFloat4[]
		};

		const re::Buffer::BufferParams descriptorIndexParams{
			.m_lifetime = re::Lifetime::SingleFrame,
			.m_stagingPool = re::Buffer::StagingPool::Temporary,
			.m_memPoolPreference = re::Buffer::MemoryPoolPreference::UploadHeap,
			.m_accessMask = re::Buffer::Access::GPURead | re::Buffer::Access::CPUWrite,
			.m_usageMask = re::Buffer::Usage::Constant,
		};

		return re::Buffer::Create("Descriptor Indexes", descriptorIndexData, descriptorIndexParams);
	}


	re::BufferInput GetInstancedBufferLUTBufferInput(re::AccelerationStructure* tlas, gr::IndexedBufferManager& ibm)
	{
		SEAssert(tlas, "Pointer is null");

		re::AccelerationStructure::TLASParams const* tlasParams =
			dynamic_cast<re::AccelerationStructure::TLASParams const*>(tlas->GetASParams());

		effect::EffectDB const& effectDB = re::RenderManager::Get()->GetEffectDB();

		const ResourceHandle transformBufferHandle = 
			ibm.GetIndexedBuffer(TransformData::s_shaderName)->GetBindlessResourceHandle(re::ViewType::SRV);
		const ResourceHandle unlitMaterialBufferHandle = 
			ibm.GetIndexedBuffer(UnlitData::s_shaderName)->GetBindlessResourceHandle(re::ViewType::SRV);
		const ResourceHandle pbrMetRoughMaterialBufferHandle = 
			ibm.GetIndexedBuffer(PBRMetallicRoughnessData::s_shaderName)->GetBindlessResourceHandle(re::ViewType::SRV);

		std::vector<gr::RenderDataID> const& blasGeoIDs = tlasParams->GetBLASGeometryRenderDataIDs();
	
		size_t geoIdx = 0;
		std::vector<InstancedBufferLUTData> initialLUTData;
		for (auto const& blas : tlasParams->GetBLASInstances())
		{
			re::AccelerationStructure::BLASParams const* blasParams =
				dynamic_cast<re::AccelerationStructure::BLASParams const*>(blas->GetASParams());

			for (auto const& geometry : blasParams->m_geometry)
			{
				SEAssert(blasGeoIDs[geoIdx++] == geometry.GetRenderDataID(), "Geometry and IDs are out of sync");

				effect::Effect const* geoEffect = effectDB.GetEffect(geometry.GetEffectID());
				ResourceHandle materialResourceHandle = INVALID_RESOURCE_IDX;
				if (geoEffect->UsesBuffer(PBRMetallicRoughnessData::s_shaderName))
				{
					materialResourceHandle = pbrMetRoughMaterialBufferHandle;
				}
				else if (geoEffect->UsesBuffer(UnlitData::s_shaderName))
				{
					materialResourceHandle = unlitMaterialBufferHandle;
				}
				SEAssert(materialResourceHandle != INVALID_RESOURCE_IDX, "Failed to find a material resource handle");
				
				SEAssert(effectDB.GetEffect(geometry.GetEffectID())->UsesBuffer(TransformData::s_shaderName),
					"Effect does not use TransformData. This is unexpected");

				initialLUTData.emplace_back(InstancedBufferLUTData{
					.g_materialIndexes = glm::uvec4(materialResourceHandle, 0, 0, 0),
					.g_transformIndexes = glm::uvec4(transformBufferHandle, 0, 0, 0),
				});
			}
		}
		return ibm.GetLUTBufferInput<InstancedBufferLUTData>(
			InstancedBufferLUTData::s_shaderName, std::move(initialLUTData), blasGeoIDs);
	}
}

namespace gr
{
	RayTracing_ExperimentalGraphicsSystem::RayTracing_ExperimentalGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(GetScriptName(), owningGSM)
		, INamedObject(GetScriptName())
		, m_rayGenIdx(0)
		, m_missShaderIdx(0)
		, m_geometryInstanceMask(re::AccelerationStructure::InstanceInclusionMask_Always)
	{
	}


	void RayTracing_ExperimentalGraphicsSystem::RegisterInputs()
	{
		RegisterDataInput(k_sceneTLASInput);
	}


	void RayTracing_ExperimentalGraphicsSystem::RegisterOutputs()
	{
		RegisterTextureOutput("RayTracingTarget", &m_rtTarget);
	}


	void RayTracing_ExperimentalGraphicsSystem::InitPipeline(
		re::StagePipeline& pipeline,
		TextureDependencies const&, 
		BufferDependencies const&, 
		DataDependencies const& dataDependencies)
	{
		m_stagePipeline = &pipeline;

		m_sceneTLAS = GetDataDependency<TLAS>(k_sceneTLASInput, dataDependencies);
		SEAssert(m_sceneTLAS, "Scene TLAS ptr cannot be null");


		// Ray tracing stage:
		m_rtStage = re::Stage::CreateRayTracingStage("RayTracing_Experimental", re::Stage::RayTracingStageParams{});
		
		// Add the camera buffer:
		m_rtStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());

		// Add a UAV target:
		m_rtTarget = re::Texture::Create("RayTracing_Experimental_Target",
			re::Texture::TextureParams{
				.m_width = static_cast<uint32_t>(core::Config::Get()->GetValue<int>(core::configkeys::k_windowWidthKey)),
				.m_height = static_cast<uint32_t>(core::Config::Get()->GetValue<int>(core::configkeys::k_windowHeightKey)),
				.m_numMips = 1,
				.m_usage = static_cast<re::Texture::Usage>(re::Texture::Usage::ColorSrc | re::Texture::Usage::ColorTarget),
				.m_dimension = re::Texture::Dimension::Texture2D,
				.m_format = re::Texture::Format::RGBA32F,
				.m_colorSpace = re::Texture::ColorSpace::Linear,
				.m_mipMode = re::Texture::MipMode::None,
			});
		m_rtStage->AddPermanentRWTextureInput("gOutput", m_rtTarget, re::TextureView(m_rtTarget));

		pipeline.AppendStage(m_rtStage);
	}


	void RayTracing_ExperimentalGraphicsSystem::PreRender()
	{
		// If the TLAS is valid, create a ray tracing batch:
		if (m_sceneTLAS && *m_sceneTLAS)
		{
			re::Batch::RayTracingParams rtParams{};
			rtParams.m_operation = re::Batch::RayTracingParams::Operation::DispatchRays;
			rtParams.m_ASInput = re::ASInput("SceneBVH", *m_sceneTLAS);
			rtParams.m_dispatchDimensions = glm::uvec3(
				static_cast<uint32_t>(core::Config::Get()->GetValue<int>(core::configkeys::k_windowWidthKey)),
				static_cast<uint32_t>(core::Config::Get()->GetValue<int>(core::configkeys::k_windowHeightKey)),
				1u);
			rtParams.m_rayGenShaderIdx = m_rayGenIdx;

			re::Batch* rtBatch = m_rtStage->AddBatch(re::Batch(re::Lifetime::SingleFrame, rtParams));

			// Attach indexed buffer LUT to the batch:
			re::BufferInput const& indexedBufferLUT = GetInstancedBufferLUTBufferInput(
				(*m_sceneTLAS).get(),
				m_graphicsSystemManager->GetRenderData().GetInstancingIndexedBufferManager());

			rtBatch->SetBuffer(indexedBufferLUT);

			// Descriptor indexes buffer:
			std::shared_ptr<re::Buffer> descriptorIndexes = CreateDescriptorIndexesBuffer(
				(*m_sceneTLAS)->GetBindlessVertexStreamLUT().GetBuffer()->GetBindlessResourceHandle(re::ViewType::SRV),
				indexedBufferLUT.GetBuffer()->GetBindlessResourceHandle(re::ViewType::SRV),
				m_graphicsSystemManager->GetActiveCameraParams().GetBuffer()->GetBindlessResourceHandle(re::ViewType::CBV),
				m_rtTarget->GetBindlessResourceHandle(re::ViewType::UAV));

			rtBatch->SetBuffer(DescriptorIndexData::s_shaderName, descriptorIndexes);

			// Ray tracing params:
			std::shared_ptr<re::Buffer> const& traceRayParams = CreateTraceRayParams(
				m_geometryInstanceMask,
				RayFlag::None,
				m_missShaderIdx);

			// Note: We currently only set our TraceRayParams buffer on the m_rtStage to maintain its lifetime; RT uses
			// bindless resources so the buffer is not directly bound
			m_rtStage->AddSingleFrameBuffer(re::BufferInput("TraceRayParams", traceRayParams));

			SEAssert((*m_sceneTLAS)->GetResourceHandle() != INVALID_RESOURCE_IDX &&
				traceRayParams->GetBindlessResourceHandle(re::ViewType::CBV) != INVALID_RESOURCE_IDX &&
				descriptorIndexes->GetBindlessResourceHandle(re::ViewType::CBV) != INVALID_RESOURCE_IDX,
				"Invalid resource handle detected");

			// Set root constants for the frame:
			glm::uvec4 rootConstants(
				(*m_sceneTLAS)->GetResourceHandle(),								// SceneBVH[]
				traceRayParams->GetBindlessResourceHandle(re::ViewType::CBV),		// TraceRayParams[]
				descriptorIndexes->GetBindlessResourceHandle(re::ViewType::CBV),	// DescriptorIndexes[]
				0);																	// unused
			
			m_rtStage->SetRootConstant("GlobalConstants", &rootConstants, re::DataType::UInt4);
		}
		else
		{
			std::shared_ptr<re::ClearRWTexturesStage> clearStage =
				re::Stage::CreateSingleFrameRWTextureClearStage("RayTracing_Experimental Target clear stage");

			clearStage->AddSingleFrameRWTextureInput(m_rtTarget, re::TextureView(m_rtTarget));
			clearStage->SetClearValue(glm::vec4(0.f));

			m_stagePipeline->AppendSingleFrameStage(clearStage);
		}
	}


	void RayTracing_ExperimentalGraphicsSystem::ShowImGuiWindow()
	{
		re::AccelerationStructure::TLASParams const* tlasParams =
			dynamic_cast<re::AccelerationStructure::TLASParams const*>((*m_sceneTLAS)->GetASParams());
		SEAssert(tlasParams, "Failed to cast to TLASParams");

		// Ray gen shader:
		const uint32_t numRayGenStyles = util::CheckedCast<uint32_t>(
			tlasParams->GetShaderBindingTable()->GetSBTParams().m_rayGenStyles.size());

		std::vector<std::string> rayGenComboOptions;
		rayGenComboOptions.reserve(numRayGenStyles);
		for (uint32_t i = 0; i < numRayGenStyles; ++i)
		{
			rayGenComboOptions.emplace_back(std::format("{}", i));
		}

		static uint32_t curRayGenIdx = m_rayGenIdx;
		util::ShowBasicComboBox("Ray gen shader index", rayGenComboOptions.data(), numRayGenStyles, m_rayGenIdx);


		// Miss shader:
		const uint32_t numMissStyles = util::CheckedCast<uint32_t>(
			tlasParams->GetShaderBindingTable()->GetSBTParams().m_missStyles.size());

		std::vector<std::string> comboOptions;
		comboOptions.reserve(numMissStyles);
		for (uint32_t i = 0; i < numMissStyles; ++i)
		{
			comboOptions.emplace_back(std::format("{}", i));
		}

		static uint32_t curMissIdx = m_missShaderIdx;
		util::ShowBasicComboBox("Miss shader index", comboOptions.data(), numMissStyles, m_missShaderIdx);

		// Geometry inclusion masks:
		auto SetInclusionMaskBits = [this](re::AccelerationStructure::InclusionMask flag, bool enabled)
			{
				if (enabled)
				{
					m_geometryInstanceMask |= flag;
				}
				else
				{
					m_geometryInstanceMask &= (re::AccelerationStructure::InstanceInclusionMask_Always ^ flag);
				}
			};

		static bool s_alphaMode_Opaque = m_geometryInstanceMask & re::AccelerationStructure::AlphaMode_Opaque;
		if (ImGui::Checkbox("AlphaMode_Opaque", &s_alphaMode_Opaque))
		{
			SetInclusionMaskBits(re::AccelerationStructure::AlphaMode_Opaque, s_alphaMode_Opaque);
		}

		static bool s_alphaMode_Mask = m_geometryInstanceMask & re::AccelerationStructure::AlphaMode_Mask;
		if (ImGui::Checkbox("AlphaMode_Mask", &s_alphaMode_Mask))
		{
			SetInclusionMaskBits(re::AccelerationStructure::AlphaMode_Mask, s_alphaMode_Mask);
		}

		static bool s_alphaMode_Blend = m_geometryInstanceMask & re::AccelerationStructure::AlphaMode_Blend;
		if (ImGui::Checkbox("AlphaMode_Blend", &s_alphaMode_Blend))
		{
			SetInclusionMaskBits(re::AccelerationStructure::AlphaMode_Blend, s_alphaMode_Blend);
		}

		static bool s_singleSided = m_geometryInstanceMask & re::AccelerationStructure::SingleSided;
		if (ImGui::Checkbox("SingleSided", &s_singleSided))
		{
			SetInclusionMaskBits(re::AccelerationStructure::SingleSided, s_singleSided);
		}

		static bool s_doubleSided = m_geometryInstanceMask & re::AccelerationStructure::DoubleSided;
		if (ImGui::Checkbox("DoubleSided", &s_doubleSided))
		{
			SetInclusionMaskBits(re::AccelerationStructure::DoubleSided, s_doubleSided);
		}

		static bool s_noShadow = m_geometryInstanceMask & re::AccelerationStructure::NoShadow;
		if (ImGui::Checkbox("NoShadow", &s_noShadow))
		{
			SetInclusionMaskBits(re::AccelerationStructure::NoShadow, s_noShadow);
		}

		static bool s_shadowCaster = m_geometryInstanceMask & re::AccelerationStructure::ShadowCaster;
		if (ImGui::Checkbox("ShadowCaster", &s_shadowCaster))
		{
			SetInclusionMaskBits(re::AccelerationStructure::ShadowCaster, s_shadowCaster);
		}

		// LUT buffer debugging:
		if (ImGui::CollapsingHeader("Instanced Buffer LUT debugging"))
		{
			ImGui::Indent();

			re::AccelerationStructure::TLASParams const* tlasParams =
				dynamic_cast<re::AccelerationStructure::TLASParams const*>((*m_sceneTLAS)->GetASParams());

			std::vector<gr::RenderDataID> const& blasGeoIDs = tlasParams->GetBLASGeometryRenderDataIDs();

			std::vector<InstancedBufferLUTData> instancedBufferLUTData(blasGeoIDs.size());
			m_graphicsSystemManager->GetRenderData().GetInstancingIndexedBufferManager().GetLUTBufferData(
				instancedBufferLUTData,
				blasGeoIDs);

			SEAssert(blasGeoIDs.size() == instancedBufferLUTData.size(), "Size mismatch");

			for (size_t i = 0; i < blasGeoIDs.size(); ++i)
			{
				ImGui::Text(std::format("BLAS Geometry RenderDataID: {}", blasGeoIDs[i]).c_str());

				InstancedBufferLUTData const& lutEntry = instancedBufferLUTData[i];

				ImGui::Text(std::format("Material resource index: {}", lutEntry.g_materialIndexes.x).c_str());
				ImGui::Text(std::format("Material buffer index: {}", lutEntry.g_materialIndexes.y).c_str());
				ImGui::Text(std::format("Material type: {}",
					gr::Material::MaterialIDToNameCStr(
						static_cast<gr::Material::MaterialID>(lutEntry.g_materialIndexes.z))).c_str());

				ImGui::Text(std::format("Transform resource index: {}", lutEntry.g_transformIndexes.x).c_str());
				ImGui::Text(std::format("Transform buffer index: {}", lutEntry.g_transformIndexes.y).c_str());

				ImGui::Separator();
			}
			
			ImGui::Unindent();
		}
	}
}