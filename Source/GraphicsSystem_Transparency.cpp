// © 2024 Adam Badke. All rights reserved.
#include "BatchManager.h"
#include "GraphicsSystem_Transparency.h"
#include "GraphicsSystemManager.h"
#include "Sampler.h"

#include "Core/Config.h"

#include "Shaders/Common/LightParams.h"


namespace gr
{
	TransparencyGraphicsSystem::TransparencyGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(GetScriptName(), owningGSM)
		, INamedObject(GetScriptName())
		, m_ambientIEMTex(nullptr)
		, m_ambientPMREMTex(nullptr)
		, m_ambientParams(nullptr)
	{
	}


	void TransparencyGraphicsSystem::RegisterInputs()
	{
		RegisterTextureInput(k_sceneDepthTexInput);
		RegisterTextureInput(k_sceneLightingTexInput);
		RegisterTextureInput(k_ambientIEMTexInput, TextureInputDefault::CubeMap_OpaqueBlack);
		RegisterTextureInput(k_ambientPMREMTexInput, TextureInputDefault::CubeMap_OpaqueBlack);
		RegisterTextureInput(k_ambientDFGTexInput);

		RegisterBufferInput(k_ambientParamsBufferInput);

		RegisterDataInput(k_cullingDataInput);
	}


	void TransparencyGraphicsSystem::RegisterOutputs()
	{
		//
	}


	void TransparencyGraphicsSystem::InitPipeline(
		re::StagePipeline& pipeline,
		TextureDependencies const& texDependencies,
		BufferDependencies const& bufferDependencies)
	{
		m_transparencyStage = re::RenderStage::CreateGraphicsStage("Transparency Stage", {});

		m_transparencyStage->SetBatchFilterMaskBit(
			re::Batch::Filter::AlphaBlended, re::RenderStage::FilterMode::Require, true);

		m_transparencyStage->SetDrawStyle(effect::DrawStyle::RenderPath_Forward);


		// Targets:
		std::shared_ptr<re::TextureTargetSet> transparencyTarget = re::TextureTargetSet::Create("Transparency Targets");

		SEAssert(texDependencies.at(k_sceneLightingTexInput), "Mandatory scene lighting texture input not recieved");
		transparencyTarget->SetColorTarget(0, *texDependencies.at(k_sceneLightingTexInput), re::TextureTarget::TargetParams{});

		re::TextureTarget::TargetParams depthTargetParams;
		depthTargetParams.m_channelWriteMode.R = re::TextureTarget::TargetParams::ChannelWrite::Disabled;

		transparencyTarget->SetDepthStencilTarget(
			*texDependencies.at(k_sceneDepthTexInput),
			depthTargetParams);

		transparencyTarget->SetAllColorTargetBlendModes(re::TextureTarget::TargetParams::BlendModes{
			re::TextureTarget::TargetParams::BlendMode::SrcAlpha, re::TextureTarget::TargetParams::BlendMode::OneMinusSrcAlpha });

		m_transparencyStage->SetTextureTargetSet(transparencyTarget);

		// Buffers:		
		m_transparencyStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());
		m_transparencyStage->AddPermanentBuffer(transparencyTarget->GetCreateTargetParamsBuffer());

		// Inputs:
		SEAssert(texDependencies.contains(k_ambientIEMTexInput) &&
			texDependencies.at(k_ambientIEMTexInput) != nullptr &&
			texDependencies.contains(k_ambientPMREMTexInput) &&
			texDependencies.at(k_ambientPMREMTexInput) != nullptr &&
			bufferDependencies.contains(k_ambientParamsBufferInput) &&
			bufferDependencies.at(k_ambientParamsBufferInput) != nullptr,
			"Missing a required input: We should at least receive some defaults");

		// Texture inputs:
		SEAssert(texDependencies.at(k_ambientDFGTexInput), "Ambient DFG texture not received");
		m_transparencyStage->AddPermanentTextureInput(
			"DFG",
			texDependencies.at(k_ambientDFGTexInput)->get(),
			re::Sampler::GetSampler("ClampMinMagMipPoint"));

		// Cache the pointers in case the light changes change
		m_ambientIEMTex = texDependencies.at(k_ambientIEMTexInput);
		m_ambientPMREMTex = texDependencies.at(k_ambientPMREMTexInput);
		m_ambientParams = bufferDependencies.at(k_ambientParamsBufferInput);

		pipeline.AppendRenderStage(m_transparencyStage);
	}


	void TransparencyGraphicsSystem::PreRender(DataDependencies const& dataDependencies)
	{
		SEAssert(m_ambientIEMTex && m_ambientPMREMTex && m_ambientParams,
			"Required inputs are null: We should at least have received any empty pointer");

		// Add our inputs each frame in case the light changes/they're updated by the source GS
		if (*m_ambientIEMTex && *m_ambientPMREMTex && *m_ambientParams)
		{
			m_transparencyStage->AddSingleFrameTextureInput(
				"CubeMapIEM",
				*m_ambientIEMTex,
				re::Sampler::GetSampler("WrapMinMagMipLinear"));

			m_transparencyStage->AddSingleFrameTextureInput(
				"CubeMapPMREM",
				*m_ambientPMREMTex,
				re::Sampler::GetSampler("WrapMinMagMipLinear"));

			m_transparencyStage->AddSingleFrameBuffer(*m_ambientParams);
		}
		else
		{
			m_transparencyStage->AddSingleFrameBuffer(re::Buffer::Create(
				AmbientLightData::s_shaderName,
				GetAmbientLightParamsData(
					1,
					0.f,
					0.f,
					static_cast<uint32_t>(core::Config::Get()->GetValue<int>(core::configkeys::k_brdfLUTWidthHeightKey)),
					nullptr),
				re::Buffer::Type::SingleFrame));
		}

		gr::BatchManager const& batchMgr = m_graphicsSystemManager->GetBatchManager();

		ViewCullingResults const* cullingResults =
			static_cast<ViewCullingResults const*>(dataDependencies.at(k_cullingDataInput));

		if (cullingResults)
		{
			const gr::RenderDataID mainCamID = m_graphicsSystemManager->GetActiveCameraRenderDataID();

			std::vector<re::Batch> const& sceneBatches = batchMgr.GetSceneBatches(
				cullingResults->at(mainCamID),
				(gr::BatchManager::InstanceType::Transform | gr::BatchManager::InstanceType::Material),
				re::Batch::Filter::AlphaBlended);

			m_transparencyStage->AddBatches(sceneBatches);
		}
		else
		{
			std::vector<re::Batch> const& allSceneBatches = batchMgr.GetAllSceneBatches(
				(gr::BatchManager::InstanceType::Transform | gr::BatchManager::InstanceType::Material),
				re::Batch::Filter::AlphaBlended);

			m_transparencyStage->AddBatches(allSceneBatches);
		}
	}
}