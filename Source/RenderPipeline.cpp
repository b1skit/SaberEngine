// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "RenderPipeline.h"

using re::RenderStage;
using std::vector;


namespace re
{
	/******************************************** StagePipeline********************************************/

	std::vector<re::RenderStage*>::iterator StagePipeline::AppendRenderStage(RenderStage* renderStage)
	{
		SEAssert("Cannot append a null RenderStage", renderStage);
		
		m_renderStages.emplace_back(renderStage);
		return m_renderStages.end();
	}


	std::vector<re::RenderStage>::iterator StagePipeline::AppendSingleFrameRenderStage(RenderStage const& renderStage)
	{	
		m_singleFrameRenderStages.emplace_back(renderStage);
		return m_singleFrameRenderStages.end();
	}


	void StagePipeline::EndOfFrame()
	{
		m_singleFrameRenderStages.clear();

		for (RenderStage* renderStage : m_renderStages)
		{
			renderStage->EndOfFrame();
		}
	}


	void StagePipeline::Destroy()
	{
		m_renderStages.clear();
		m_singleFrameRenderStages.clear();
	}


	/******************************************** RenderPipeline********************************************/

	StagePipeline& RenderPipeline::AddNewStagePipeline(std::string stagePipelineName)
	{ 
		m_stagePipeline.emplace_back(stagePipelineName);
		return m_stagePipeline.back();
	}


	void RenderPipeline::Destroy()
	{
		m_stagePipeline.clear();
	}
}