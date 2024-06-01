// © 2024 Adam Badke. All rights reserved.
#include "BatchManager.h"
#include "GraphicsSystem_Transparency.h"
#include "GraphicsSystemManager.h"


namespace gr
{
	TransparencyGraphicsSystem::TransparencyGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(GetScriptName(), owningGSM)
		, INamedObject(GetScriptName())
	{
	}


	void TransparencyGraphicsSystem::RegisterInputs()
	{
		RegisterTextureInput(k_sceneDepthTexInput);
		RegisterTextureInput(k_sceneLightingTexInput);

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

		// Camera:		
		m_transparencyStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());

		// Targets:
		std::shared_ptr<re::TextureTargetSet> transparencyTarget = re::TextureTargetSet::Create("Transparency Targets");

		SEAssert(texDependencies.at(k_sceneLightingTexInput), "Mandatory scene lighting texture input not recieved");
		transparencyTarget->SetColorTarget(0, texDependencies.at(k_sceneLightingTexInput), re::TextureTarget::TargetParams{});

		re::TextureTarget::TargetParams depthTargetParams;
		depthTargetParams.m_channelWriteMode.R = re::TextureTarget::TargetParams::ChannelWrite::Disabled;

		transparencyTarget->SetDepthStencilTarget(
			texDependencies.at(k_sceneDepthTexInput),
			depthTargetParams);

		transparencyTarget->SetAllColorTargetBlendModes(re::TextureTarget::TargetParams::BlendModes{
			re::TextureTarget::TargetParams::BlendMode::SrcAlpha, re::TextureTarget::TargetParams::BlendMode::OneMinusSrcAlpha });

		m_transparencyStage->SetTextureTargetSet(transparencyTarget);


		pipeline.AppendRenderStage(m_transparencyStage);
	}


	void TransparencyGraphicsSystem::PreRender(DataDependencies const& dataDependencies)
	{
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