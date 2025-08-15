// © 2025 Adam Badke. All rights reserved.
#include "AccelerationStructure.h"
#include "Batch.h"
#include "BatchBuilder.h"
#include "BatchHandle.h"
#include "Buffer.h"
#include "BufferView.h"
#include "EnumTypes.h"
#include "GraphicsSystem.h"
#include "GraphicsSystem_ReferencePathTracer.h"
#include "GraphicsSystemCommon.h"
#include "GraphicsSystemManager.h"
#include "IndexedBuffer.h"
#include "LightRenderData.h"
#include "RayTracingParamsHelpers.h"
#include "RenderDataManager.h"
#include "RenderObjectIDs.h"
#include "RenderPipeline.h"
#include "ShaderBindingTable.h"
#include "Stage.h"
#include "TextureView.h"

#include "Core/Assert.h"
#include "Core/Config.h"
#include "Core/Logger.h"

#include "Core/Interfaces/INamedObject.h"

#include "Renderer/Shaders/Common/RayTracingParams.h"
#include "Renderer/Shaders/Common/ResourceCommon.h"

#include "_generated/DrawStyles.h"


namespace
{
	void UpdateTemporalParams(std::shared_ptr<re::Buffer>& temporalParams, uint64_t numAccumulatedFrames)
	{
		TemporalAccumulationData temporalAccumulationData{
			.g_frameStats = glm::uvec4(
				numAccumulatedFrames,
				0,
				0,
				0),
		};

		if (temporalParams == nullptr)
		{
			temporalParams = re::Buffer::Create(
				"Temporal Accumulation Buffer",
				temporalAccumulationData,
				re::Buffer::BufferParams{
					.m_stagingPool = re::Buffer::StagingPool::Permanent,
					.m_memPoolPreference = re::Buffer::UploadHeap,
					.m_accessMask = re::Buffer::GPURead | re::Buffer::CPUWrite,
					.m_usageMask = re::Buffer::Constant,
				});
		}
		else
		{
			temporalParams->Commit(temporalAccumulationData);
		}
	}
}

namespace gr
{
	ReferencePathTracerGraphicsSystem::ReferencePathTracerGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(GetScriptName(), owningGSM)
		, INamedObject(GetScriptName())
		, m_refPathTracerEffectID(effect::Effect::ComputeEffectID("ReferencePathTracer"))
		, m_rayGenIdx(0)
		, m_missShaderIdx(0)
		, m_geometryInstanceMask(re::AccelerationStructure::InclusionMask_Always)
		, m_accumulationStartFrame(0)
		, m_numAccumulatedFrames(0)
		, m_mustResetTemporalAccumulation(true)
	{
	}


	void ReferencePathTracerGraphicsSystem::RegisterInputs()
	{
		RegisterDataInput(k_sceneTLASInput);
	}


	void ReferencePathTracerGraphicsSystem::RegisterOutputs()
	{
		RegisterTextureOutput("LightAccumulation", &m_outputAccumulation);
	}


	void ReferencePathTracerGraphicsSystem::InitPipeline(
		gr::StagePipeline& pipeline,
		TextureDependencies const&,
		BufferDependencies const&,
		DataDependencies const& dataDependencies)
	{
		m_stagePipeline = &pipeline;

		m_sceneTLAS = GetDependency<TLAS>(k_sceneTLASInput, dataDependencies);

		m_stagePipelineParentItr = pipeline.AppendStage(gr::Stage::CreateParentStage("ReferencePathTracer Parent Stage"));

		// Ray tracing stage:
		m_rtStage = gr::Stage::CreateRayTracingStage("ReferencePathTracer", gr::Stage::RayTracingStageParams{});

		// Create a UAV target (Note: We access this bindlessly):
		m_workingAccumulation = re::Texture::Create("Working Light Accumulation",
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

		m_outputAccumulation = re::Texture::Create("Light Accumulation Output",
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

		auto rtStageItr = pipeline.AppendStage(m_stagePipelineParentItr, m_rtStage);

		// Copy the working accumulation to the output texture so future stages can modify it (e.g. Tonemapping):
		std::shared_ptr<gr::CopyStage> outputCopyStage =
			gr::Stage::CreateCopyStage(m_workingAccumulation, m_outputAccumulation);

		pipeline.AppendStage(rtStageItr, outputCopyStage);

		// Register for events:
		m_graphicsSystemManager->SubscribeToGraphicsEvent<ReferencePathTracerGraphicsSystem>(
			greventkey::k_triggerTemporalAccumulationReset, this);
		m_graphicsSystemManager->SubscribeToGraphicsEvent<ReferencePathTracerGraphicsSystem>(
			greventkey::k_activeAmbientLightHasChanged, this);
	}


	void ReferencePathTracerGraphicsSystem::HandleEvents()
	{
		while (HasEvents())
		{
			gr::GraphicsEvent const& event = GetEvent();
			switch (event.m_eventKey)
			{
			case greventkey::k_triggerTemporalAccumulationReset:
			{
				m_mustResetTemporalAccumulation = true;
			}
			break;
			case greventkey::k_activeAmbientLightHasChanged:
			{
				const gr::RenderDataID activeAmbientLightID = std::get<gr::RenderDataID>(event.m_data);

				if (activeAmbientLightID != gr::k_invalidRenderDataID)
				{
					gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();

					gr::Light::RenderDataAmbientIBL const& ambientRenderData =
						renderData.GetObjectData<gr::Light::RenderDataAmbientIBL>(activeAmbientLightID);

					m_environmentMap = ambientRenderData.m_iblTex;
				}
				else
				{
					m_environmentMap = nullptr;
				}
			}
			break;
			default: SEAssertF("Unexpected graphics event in ReferencePathTracerGraphicsSystem");
			}
		}
	}


	void ReferencePathTracerGraphicsSystem::PreRender()
	{
		HandleEvents();

		if (m_mustResetTemporalAccumulation ||
			(m_sceneTLAS && *m_sceneTLAS) == false)
		{
			std::shared_ptr<gr::ClearRWTexturesStage> clearStage =
				gr::Stage::CreateSingleFrameRWTextureClearStage("Reference Path Tracer Target clear stage");

			clearStage->AddSingleFrameRWTextureInput(m_workingAccumulation, re::TextureView(m_workingAccumulation));
			clearStage->SetClearValue(glm::vec4(0.f));

			m_stagePipeline->AppendSingleFrameStage(m_stagePipelineParentItr, clearStage);

			const uint64_t currentFrameNum = m_graphicsSystemManager->GetCurrentRenderFrameNum();
			if (currentFrameNum != m_accumulationStartFrame + 1)
			{
				LOG("Temporal accumulation reset");
			}
			m_accumulationStartFrame = currentFrameNum;
			m_numAccumulatedFrames = 0;

			m_mustResetTemporalAccumulation = false;
		}

		if (m_numAccumulatedFrames > 0 && m_numAccumulatedFrames % 1000 == 0)
		{
			LOG("Accumulated %d frames so far...", m_numAccumulatedFrames);
		}
		

		// If the TLAS is valid, create a ray tracing batch:
		if (m_sceneTLAS && *m_sceneTLAS)
		{
			if (!(*m_sceneTLAS)->HasShaderBindingTable(m_refPathTracerEffectID))
			{
				constexpr uint32_t k_maxPayloadByteSize = 
					static_cast<uint32_t>(std::max(sizeof(PathPayload), sizeof(VisibilityPayload)));

				(*m_sceneTLAS)->AddShaderBindingTable(
					m_refPathTracerEffectID,
					re::ShaderBindingTable::SBTParams{
						.m_rayGenStyles = { effect::drawstyle::RayGen_Default, },
						.m_missStyles = { effect::drawstyle::Miss_Default, },
						.m_hitgroupStyles = effect::drawstyle::HitGroup_Reference,
						.m_effectID = m_refPathTracerEffectID,
						.m_maxPayloadByteSize = k_maxPayloadByteSize,
						.m_maxRecursionDepth = 1, }); // Use iterative ray generation
			}


			UpdateTemporalParams(m_temporalParams, m_numAccumulatedFrames++);
			SEAssert(m_numAccumulatedFrames < std::numeric_limits<uint32_t>::max(),
				"Temporary accumulation frame index is about to overflow");

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
				.SetEffectID(m_refPathTracerEffectID)
				.SetRayGenShaderIdx(m_rayGenIdx)
				.Build());

			// Descriptor indexes buffer:
			std::shared_ptr<re::Buffer> const& descriptorIndexes = grutil::CreateDescriptorIndexesBuffer(
				(*m_sceneTLAS)->GetBindlessVertexStreamLUT().GetBuffer()->GetResourceHandle(re::ViewType::SRV),
				indexedBufferLUT.GetBuffer()->GetResourceHandle(re::ViewType::SRV),
				m_graphicsSystemManager->GetActiveCameraParams().GetBuffer()->GetResourceHandle(re::ViewType::CBV),
				m_workingAccumulation->GetResourceHandle(re::ViewType::UAV),
				m_environmentMap ? m_environmentMap->GetResourceHandle(re::ViewType::SRV) : INVALID_RESOURCE_IDX);

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
				descriptorIndexes->GetResourceHandle(re::ViewType::CBV) != INVALID_RESOURCE_IDX &&
				m_temporalParams->GetResourceHandle(re::ViewType::CBV) != INVALID_RESOURCE_IDX,
				"Invalid resource handle detected");

			// Set root constants for the frame:
			const glm::uvec4 rootConstants(
				(*m_sceneTLAS)->GetResourceHandle(),						// SceneBVH[]
				traceRayParams->GetResourceHandle(re::ViewType::CBV),		// TraceRayParams[]
				descriptorIndexes->GetResourceHandle(re::ViewType::CBV),	// DescriptorIndexes[]
				m_temporalParams->GetResourceHandle(re::ViewType::CBV));	// TemporalParams[]

			m_rtStage->SetRootConstant("RootConstants0", &rootConstants, re::DataType::UInt4);
		}
	}


	void ReferencePathTracerGraphicsSystem::ShowImGuiWindow()
	{
		if (!m_sceneTLAS || !*m_sceneTLAS)
		{
			ImGui::Text("No scene TLAS available");
			return;
		}

		const bool didModify = 
			(*m_sceneTLAS)->ShowImGuiWindow(m_refPathTracerEffectID, m_rayGenIdx, m_missShaderIdx, m_geometryInstanceMask);

		if (didModify)
		{
			m_mustResetTemporalAccumulation = true;
		}
	}
}