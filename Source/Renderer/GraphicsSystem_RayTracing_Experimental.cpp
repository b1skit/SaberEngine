// © 2025 Adam Badke. All rights reserved.
#include "Batch.h"
#include "BatchBuilder.h"
#include "GraphicsSystem_RayTracing_Experimental.h"
#include "GraphicsSystemManager.h"
#include "IndexedBuffer.h"
#include "Material.h"
#include "RayTracingParamsHelpers.h"
#include "RenderObjectIDs.h"
#include "ShaderBindingTable.h"

#include "Core/Config.h"

#include "Core/Util/ImGuiUtils.h"

#include "Renderer/Shaders/Common/RayTracingParams.h"
#include "Renderer/Shaders/Common/ResourceCommon.h"


namespace gr
{
	RayTracing_ExperimentalGraphicsSystem::RayTracing_ExperimentalGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(GetScriptName(), owningGSM)
		, INamedObject(GetScriptName())
		, m_rtEffectID(effect::Effect::ComputeEffectID("RayTracing_Experimental"))
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
		gr::StagePipeline& pipeline,
		TextureDependencies const&, 
		BufferDependencies const&, 
		DataDependencies const& dataDependencies)
	{
		m_stagePipeline = &pipeline;

		m_sceneTLAS = GetDataDependency<TLAS>(k_sceneTLASInput, dataDependencies);
		SEAssert(m_sceneTLAS, "Scene TLAS ptr cannot be null");


		// Ray tracing stage:
		m_rtStage = gr::Stage::CreateRayTracingStage("RayTracing_Experimental", gr::Stage::RayTracingStageParams{});
		
		// Create a UAV target (Note: We access this bindlessly):
		m_rtTarget = re::Texture::Create("RayTracing_Experimental_Target",
			re::Texture::TextureParams{
				.m_width = static_cast<uint32_t>(core::Config::GetValue<int>(core::configkeys::k_windowWidthKey)),
				.m_height = static_cast<uint32_t>(core::Config::GetValue<int>(core::configkeys::k_windowHeightKey)),
				.m_numMips = 1,
				.m_usage = re::Texture::Usage::ColorSrc | re::Texture::Usage::ColorTarget,
				.m_dimension = re::Texture::Dimension::Texture2D,
				.m_format = re::Texture::Format::RGBA32F,
				.m_colorSpace = re::Texture::ColorSpace::Linear,
				.m_mipMode = re::Texture::MipMode::None,
			});

		pipeline.AppendStage(m_rtStage);
	}


	void RayTracing_ExperimentalGraphicsSystem::PreRender()
	{
		// If the TLAS is valid, create a ray tracing batch:
		if (m_sceneTLAS && *m_sceneTLAS)
		{
			re::BufferInput const& indexedBufferLUT = grutil::GetInstancedBufferLUTBufferInput(
				(*m_sceneTLAS).get(),
				m_graphicsSystemManager->GetRenderData().GetInstancingIndexedBufferManager());

			gr::StageBatchHandle& rtBatch = *m_rtStage->AddBatch(gr::RayTraceBatchBuilder()
				.SetOperation(gr::Batch::RayTracingParams::Operation::DispatchRays)
				.SetASInput(re::ASInput("SceneBVH", *m_sceneTLAS))
				.SetDispatchDimensions(glm::uvec3(
					static_cast<uint32_t>(core::Config::GetValue<int>(core::configkeys::k_windowWidthKey)),
					static_cast<uint32_t>(core::Config::GetValue<int>(core::configkeys::k_windowHeightKey)),
					1u))
				.SetEffectID(m_rtEffectID)
				.SetRayGenShaderIdx(m_rayGenIdx)
				.Build());

			// Descriptor indexes buffer:
			std::shared_ptr<re::Buffer> const& descriptorIndexes = grutil::CreateDescriptorIndexesBuffer(
				(*m_sceneTLAS)->GetBindlessVertexStreamLUT().GetBuffer()->GetResourceHandle(re::ViewType::SRV),
				indexedBufferLUT.GetBuffer()->GetResourceHandle(re::ViewType::SRV),
				m_graphicsSystemManager->GetActiveCameraParams().GetBuffer()->GetResourceHandle(re::ViewType::CBV),
				m_rtTarget->GetResourceHandle(re::ViewType::UAV));

			// Ray tracing params:
			std::shared_ptr<re::Buffer> const& traceRayParams = grutil::CreateTraceRayParams(
				m_geometryInstanceMask,
				RayFlag::None,
				m_missShaderIdx);

			// Note: We set our Buffers on the Batch to maintain their lifetime; RT uses bindless resources so the
			// buffer is not directly bound
			rtBatch.SetSingleFrameBuffer(indexedBufferLUT);
			rtBatch.SetSingleFrameBuffer(DescriptorIndexData::s_shaderName, descriptorIndexes);			
			rtBatch.SetSingleFrameBuffer(TraceRayData::s_shaderName, traceRayParams);

			SEAssert((*m_sceneTLAS)->GetResourceHandle() != INVALID_RESOURCE_IDX &&
				traceRayParams->GetResourceHandle(re::ViewType::CBV) != INVALID_RESOURCE_IDX &&
				descriptorIndexes->GetResourceHandle(re::ViewType::CBV) != INVALID_RESOURCE_IDX,
				"Invalid resource handle detected");

			// Set root constants for the frame:
			const glm::uvec4 rootConstants(
				(*m_sceneTLAS)->GetResourceHandle(),								// SceneBVH[]
				traceRayParams->GetResourceHandle(re::ViewType::CBV),		// TraceRayParams[]
				descriptorIndexes->GetResourceHandle(re::ViewType::CBV),	// DescriptorIndexes[]
				0);																	// unused
			
			m_rtStage->SetRootConstant("RootConstants0", &rootConstants, re::DataType::UInt4);
		}
		else
		{
			std::shared_ptr<gr::ClearRWTexturesStage> clearStage =
				gr::Stage::CreateSingleFrameRWTextureClearStage("RayTracing_Experimental Target clear stage");

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

		std::shared_ptr<re::ShaderBindingTable> const& sbt = tlasParams->GetShaderBindingTable(m_rtEffectID);

		ImGui::Text("Effect Shader Binding Table: \"%s\"", sbt->GetName().c_str());

		// Ray gen shader:
		const uint32_t numRayGenStyles = util::CheckedCast<uint32_t>(sbt->GetSBTParams().m_rayGenStyles.size());

		std::vector<std::string> rayGenComboOptions;
		rayGenComboOptions.reserve(numRayGenStyles);
		for (uint32_t i = 0; i < numRayGenStyles; ++i)
		{
			rayGenComboOptions.emplace_back(std::format("{}", i));
		}

		static uint32_t curRayGenIdx = m_rayGenIdx;
		util::ShowBasicComboBox("Ray gen shader index", rayGenComboOptions.data(), numRayGenStyles, m_rayGenIdx);


		// Miss shader:
		const uint32_t numMissStyles = util::CheckedCast<uint32_t>(sbt->GetSBTParams().m_missStyles.size());

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

			std::vector<gr::RenderDataID> const& blasGeoIDs = tlasParams->GetBLASGeometryOwnerIDs();

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