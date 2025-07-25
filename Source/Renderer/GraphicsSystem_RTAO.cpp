// © 2025 Adam Badke. All rights reserved.
#include "BatchBuilder.h"
#include "Buffer.h"
#include "EnumTypes.h"
#include "GraphicsSystem_RTAO.h"
#include "GraphicsSystemCommon.h"
#include "GraphicsSystemManager.h"
#include "RayTracingParamsHelpers.h"
#include "RenderDataManager.h"
#include "RenderPipeline.h"
#include "Stage.h"
#include "TextureView.h"

#include "Core/Assert.h"
#include "Core/Config.h"
#include "Core/InvPtr.h"

#include "Core/Definitions/ConfigKeys.h"

#include "Renderer/Shaders/Common/RayTracingParams.h"
#include "Renderer/Shaders/Common/RTAOParams.h"


namespace
{
	RTAOParamsData CreateRTAOParamsData(
		glm::vec2 const& tMinMax,
		uint32_t rayCount,
		bool isEnabled,
		core::InvPtr<re::Texture> const* depthTex,
		core::InvPtr<re::Texture> const* wNormalTex)
	{
		SEAssert(depthTex && *depthTex, "Depth texture pointer cannot be null");
		SEAssert(wNormalTex && *wNormalTex, "World normal texture pointer cannot be null");

		SEAssert((*depthTex)->GetResourceHandle(re::ViewType::SRV) != INVALID_RESOURCE_IDX &&
			(*wNormalTex)->GetResourceHandle(re::ViewType::SRV) != INVALID_RESOURCE_IDX,
			"Invalid resource handle detected");

		return RTAOParamsData{
			.g_params = glm::vec4(
				tMinMax.x, 
				tMinMax.y, 
				rayCount,
				static_cast<float>(isEnabled)),
			.g_indexes = glm::uvec4(
				(*depthTex)->GetResourceHandle(re::ViewType::SRV),
				(*wNormalTex)->GetResourceHandle(re::ViewType::SRV),
				0,
				0),
		};
	}
}

namespace gr
{
	RTAOGraphicsSystem::RTAOGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(GetScriptName(), owningGSM)
		, INamedObject(GetScriptName())
		, m_RTAOEffectID(effect::Effect::ComputeEffectID("RTAO"))
		, m_geometryInstanceMask(re::AccelerationStructure::InstanceInclusionMask_Always)
		, m_isDirty(true)
		, m_tMinMax(0.0001f, 0.2f)
		, m_rayCount(6)
		, m_isEnabled(true)
	{
	}


	void RTAOGraphicsSystem::RegisterInputs()
	{
		RegisterTextureInput(k_depthInput);
		RegisterTextureInput(k_wNormalInput);

		RegisterDataInput(k_sceneTLASInput);
	}


	void RTAOGraphicsSystem::RegisterOutputs()
	{
		RegisterTextureOutput(k_aoOutput, &m_workingAOTex);
	}


	void RTAOGraphicsSystem::InitPipeline(
		gr::StagePipeline& pipeline, 
		TextureDependencies const& texDependencies, 
		BufferDependencies const&, 
		DataDependencies const& dataDependencies)
	{
		m_stagePipeline = &pipeline;

		SEAssert(texDependencies.contains(k_wNormalInput) && texDependencies.contains(k_depthInput),
			"Failed to get required texture dependencies");

		m_depthInput = texDependencies.at(k_depthInput);
		m_wNormalInput = texDependencies.at(k_wNormalInput);

		m_sceneTLAS = GetDataDependency<TLAS>(k_sceneTLASInput, dataDependencies);
		SEAssert(m_sceneTLAS, "Scene TLAS ptr cannot be null");

		// Ray tracing stage:
		m_RTAOStage = gr::Stage::CreateRayTracingStage("RTAO", gr::Stage::RayTracingStageParams{});

		// Add a UAV target:
		m_workingAOTex = re::Texture::Create("RTAOTarget",
			re::Texture::TextureParams{
				.m_width = static_cast<uint32_t>(core::Config::GetValue<int>(core::configkeys::k_windowWidthKey)),
				.m_height = static_cast<uint32_t>(core::Config::GetValue<int>(core::configkeys::k_windowHeightKey)),
				.m_numMips = 1,
				.m_usage = re::Texture::Usage::ColorSrc | re::Texture::Usage::ColorTarget,
				.m_dimension = re::Texture::Dimension::Texture2D,
				.m_format = re::Texture::Format::R8_UNORM,
				.m_colorSpace = re::Texture::ColorSpace::Linear,
				.m_mipMode = re::Texture::MipMode::None,
			});

		pipeline.AppendStage(m_RTAOStage);
	}


	void RTAOGraphicsSystem::PreRender()
	{
		if (m_RTAOParams == nullptr)
		{
			// Must create this here, as our textures won't have resource handles until after InitPipeline() is called
			m_RTAOParams = re::Buffer::Create(
				"RTAO Params",
				CreateRTAOParamsData(m_tMinMax, m_rayCount, m_isEnabled, m_depthInput, m_wNormalInput),
				re::Buffer::BufferParams{
					.m_lifetime = re::Lifetime::Permanent,
					.m_stagingPool = re::Buffer::StagingPool::Permanent,
					.m_memPoolPreference = re::Buffer::MemoryPoolPreference::DefaultHeap,
					.m_accessMask = re::Buffer::Access::GPURead,
					.m_usageMask = re::Buffer::Usage::Constant,
				});
		}
		else if (m_isDirty)
		{
			m_RTAOParams->Commit(CreateRTAOParamsData(m_tMinMax, m_rayCount, m_isEnabled, m_depthInput, m_wNormalInput));
		}		
		m_isDirty = false;


		// If the TLAS is valid, create a ray tracing batch:
		if (m_sceneTLAS && *m_sceneTLAS)
		{
			re::BufferInput const& indexedBufferLUT = grutil::GetInstancedBufferLUTBufferInput(
				(*m_sceneTLAS).get(),
				m_graphicsSystemManager->GetRenderData().GetInstancingIndexedBufferManager());

			gr::StageBatchHandle& rtBatch = *m_RTAOStage->AddBatch(gr::RayTraceBatchBuilder()
				.SetOperation(gr::Batch::RayTracingParams::Operation::DispatchRays)
				.SetASInput(re::ASInput("SceneBVH", *m_sceneTLAS))
				.SetDispatchDimensions(glm::uvec3(
					static_cast<uint32_t>(core::Config::GetValue<int>(core::configkeys::k_windowWidthKey)),
					static_cast<uint32_t>(core::Config::GetValue<int>(core::configkeys::k_windowHeightKey)),
					1u))
				.SetEffectID(m_RTAOEffectID)
				.SetRayGenShaderIdx(0) // Ray generation shader index
				.Build());

			// Descriptor indexes buffer:
			std::shared_ptr<re::Buffer> const& descriptorIndexes = grutil::CreateDescriptorIndexesBuffer(
				(*m_sceneTLAS)->GetBindlessVertexStreamLUT().GetBuffer()->GetResourceHandle(re::ViewType::SRV),
				indexedBufferLUT.GetBuffer()->GetResourceHandle(re::ViewType::SRV),
				m_graphicsSystemManager->GetActiveCameraParams().GetBuffer()->GetResourceHandle(re::ViewType::CBV),
				m_workingAOTex->GetResourceHandle(re::ViewType::UAV));

			// Ray tracing params:
			std::shared_ptr<re::Buffer> const& traceRayParams = grutil::CreateTraceRayParams(
				m_geometryInstanceMask,
				RayFlag::AcceptFirstHitAndEndSearch | RayFlag::SkipClosestHitShader,
				0); // Miss shader index

			// Note: We set our Buffers on the Batch to maintain their lifetime; RT uses bindless resources so the
			// buffer is not directly bound
			rtBatch.SetSingleFrameBuffer(indexedBufferLUT);
			rtBatch.SetSingleFrameBuffer(DescriptorIndexData::s_shaderName, descriptorIndexes);
			rtBatch.SetSingleFrameBuffer(TraceRayData::s_shaderName, traceRayParams);

			// Set root constants for the frame:
			SEAssert((*m_sceneTLAS)->GetResourceHandle() != INVALID_RESOURCE_IDX &&
				traceRayParams->GetResourceHandle(re::ViewType::CBV) != INVALID_RESOURCE_IDX &&
				descriptorIndexes->GetResourceHandle(re::ViewType::CBV) != INVALID_RESOURCE_IDX &&
				m_RTAOParams->GetResourceHandle(re::ViewType::CBV) != INVALID_RESOURCE_IDX,
				"Invalid resource handle detected");

			const glm::uvec4 rtaoConstants0(
				(*m_sceneTLAS)->GetResourceHandle(),						// SceneBVH[]
				traceRayParams->GetResourceHandle(re::ViewType::CBV),		// TraceRayParams[]
				descriptorIndexes->GetResourceHandle(re::ViewType::CBV),	// DescriptorIndexes[]
				m_RTAOParams->GetResourceHandle(re::ViewType::CBV));		// RTAOParams[]	
			m_RTAOStage->SetRootConstant("RootConstants0", &rtaoConstants0, re::DataType::UInt4);
		}
		else
		{
			std::shared_ptr<gr::ClearRWTexturesStage> clearStage =
				gr::Stage::CreateSingleFrameRWTextureClearStage("RTAO Target clear");

			clearStage->AddSingleFrameRWTextureInput(m_workingAOTex, re::TextureView(m_workingAOTex));
			clearStage->SetClearValue(glm::vec4(1.f));

			m_stagePipeline->AppendSingleFrameStage(clearStage);
		}
	}


	void RTAOGraphicsSystem::ShowImGuiWindow()
	{
		m_isDirty |= ImGui::Checkbox("Enabled", &m_isEnabled);

		// Present our TMin/TMax ray interval as a base offset and ray length:
		float rayLength = m_tMinMax.y - m_tMinMax.x;

		if (ImGui::SliderFloat("Ray offset", &m_tMinMax.x, 0.f, 10.f, "%.5f"))
		{
			m_tMinMax.y = m_tMinMax.x + rayLength;
			m_isDirty = true;
		}
		
		if (ImGui::SliderFloat("Ray length", &rayLength, 0.f, 10.f, "%.5f"))
		{
			m_tMinMax.y = m_tMinMax.x + rayLength;
			m_isDirty = true;
		}

		m_isDirty |= ImGui::SliderInt("Ray count", reinterpret_cast<int*>(&m_rayCount), 1, 64);
	}
}