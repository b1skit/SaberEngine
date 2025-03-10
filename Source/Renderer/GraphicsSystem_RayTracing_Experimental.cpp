// © 2025 Adam Badke. All rights reserved.
#include "GraphicsSystem_RayTracing_Experimental.h"
#include "GraphicsSystemManager.h"
#include "ShaderBindingTable.h"

#include "Core/Config.h"

#include "Core/Util/ImGuiUtils.h"

#include "Shaders/Common/RayTracingParams.h"


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
			.m_stagingPool = re::Buffer::StagingPool::Temporary,
			.m_usageMask = re::Buffer::Usage::Constant,
		};

		return re::Buffer::Create("Trace Ray Params", traceRayData, traceRayBufferParams);
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


	RayTracing_ExperimentalGraphicsSystem::~RayTracing_ExperimentalGraphicsSystem()
	{
		if (m_sceneSBT)
		{
			m_sceneSBT->Destroy();
		}
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
		m_sceneTLAS = GetDataDependency<TLAS>(k_sceneTLASInput, dataDependencies);
		SEAssert(m_sceneTLAS, "Scene TLAS ptr cannot be null");

		// Create a shader binding table to reflect our local use of the TLAS:
		const EffectID rtEffectID = effect::Effect::ComputeEffectID("RayTracing");

		const re::ShaderBindingTable::SBTParams sbtParams{
			.m_rayGenStyles = { 
				{rtEffectID, effect::drawstyle::RT_Experimental_RT_Experimental_RayGen_A},
				{rtEffectID, effect::drawstyle::RT_Experimental_RT_Experimental_RayGen_B},
			},
			.m_missStyles = { 
				{rtEffectID, effect::drawstyle::RT_Experimental_RT_Experimental_Miss_Blue},
				{rtEffectID, effect::drawstyle::RT_Experimental_RT_Experimental_Miss_Red},
			},
			.m_hitgroupStyles = effect::drawstyle::RT_Experimental_RT_Experimental_Geometry,
			.m_maxPayloadByteSize = sizeof(HitInfo_Experimental),
			.m_maxRecursionDepth = 2,
		};
		m_sceneSBT = re::ShaderBindingTable::Create("Scene SBT", sbtParams);

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
		// Update the SBT every frame (in case the scene TLAS has been (re)created/modified etc)
		m_sceneSBT->Update(*m_sceneTLAS);

		// If the TLAS is valid, create a ray tracing batch:
		if (m_sceneTLAS && *m_sceneTLAS)
		{
			re::Batch::RayTracingParams rtParams;
			rtParams.m_operation = re::Batch::RayTracingParams::Operation::DispatchRays;
			rtParams.m_ASInput = re::ASInput("SceneBVH", *m_sceneTLAS);
			rtParams.m_shaderBindingTable = m_sceneSBT;
			rtParams.m_dispatchDimensions = glm::uvec3(
				static_cast<uint32_t>(core::Config::Get()->GetValue<int>(core::configkeys::k_windowWidthKey)),
				static_cast<uint32_t>(core::Config::Get()->GetValue<int>(core::configkeys::k_windowHeightKey)),
				1u);
			rtParams.m_rayGenShaderIdx = m_rayGenIdx;

			m_rtStage->AddBatch(re::Batch(re::Lifetime::SingleFrame, rtParams));

			// Ray tracing params:
			m_rtStage->AddSingleFrameBuffer(re::BufferInput(
				"TraceRayParams", 
				CreateTraceRayParams(
					m_geometryInstanceMask,
					RayFlag::None, 
					m_missShaderIdx)));
		}
	}


	void RayTracing_ExperimentalGraphicsSystem::ShowImGuiWindow()
	{
		// Ray gen shader:
		const uint32_t numRayGenStyles = util::CheckedCast<uint32_t>(m_sceneSBT->GetSBTParams().m_rayGenStyles.size());

		std::vector<std::string> rayGenComboOptions;
		rayGenComboOptions.reserve(numRayGenStyles);
		for (uint32_t i = 0; i < numRayGenStyles; ++i)
		{
			rayGenComboOptions.emplace_back(std::format("{}", i));
		}

		static uint32_t curRayGenIdx = m_rayGenIdx;
		util::ShowBasicComboBox("Ray gen shader index", rayGenComboOptions.data(), numRayGenStyles, m_rayGenIdx);


		// Miss shader:
		const uint32_t numMissStyles = util::CheckedCast<uint32_t>(m_sceneSBT->GetSBTParams().m_missStyles.size());

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
	}
}