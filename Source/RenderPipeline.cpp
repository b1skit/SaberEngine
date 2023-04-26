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
		
		m_stagePipeline.emplace_back(renderStage);
		return m_stagePipeline.end();
	}


	std::vector<re::RenderStage>::iterator StagePipeline::AppendSingleFrameRenderStage(RenderStage const& renderStage)
	{	
		m_singleFrameStagePipeline.emplace_back(renderStage);
		return m_singleFrameStagePipeline.end();
	}


	void StagePipeline::EndOfFrame()
	{
		m_singleFrameStagePipeline.clear();

		for (RenderStage* renderStage : m_stagePipeline)
		{
			renderStage->EndOfFrame();
		}
	}


	void StagePipeline::Destroy()
	{
		m_stagePipeline.clear();
		m_singleFrameStagePipeline.clear();
	}


	/******************************************** RenderPipeline********************************************/

	StagePipeline& RenderPipeline::AddNewStagePipeline(std::string stagePipelineName)
	{ 
		m_pipeline.emplace_back(stagePipelineName); 
		return m_pipeline.back();
	}


	void RenderPipeline::Destroy()
	{
		m_pipeline.clear();
	}
}