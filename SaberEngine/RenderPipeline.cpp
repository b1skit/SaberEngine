#pragma once

#include "RenderPipeline.h"

using gr::RenderStage;
using std::vector;

namespace re
{
	/******************************************** StagePipeline********************************************/

	std::vector<gr::RenderStage const*>::iterator StagePipeline::AppendRenderStage(RenderStage const& renderStage)
	{
		return AddRenderStageInternal(renderStage, m_stagePipeline);
	}


	std::vector<gr::RenderStage const*>::iterator StagePipeline::AppendSingleFrameRenderStage(RenderStage const& renderStage)
	{
		return AddRenderStageInternal(renderStage, m_singleFrameStagePipeline);
	}


	std::vector<gr::RenderStage const*>::iterator StagePipeline::AddRenderStageInternal(
		RenderStage const& renderStage, vector<RenderStage const*>& pipeline)
	{
		SEAssert("renderStage not fully configured",
			renderStage.GetName() != "" &&
			renderStage.GetStageShader() != nullptr &&
			renderStage.GetStageCamera() != nullptr
			// TODO: Add more conditions
		);

		pipeline.emplace_back(&renderStage);
		return pipeline.end();
	}


	void StagePipeline::EndOfFrame()
	{
		m_singleFrameStagePipeline.clear();
	}


	/******************************************** RenderPipeline********************************************/

	StagePipeline& RenderPipeline::AddNewStagePipeline(std::string stagePipelineName)
	{ 
		m_pipeline.emplace_back(stagePipelineName); 
		return m_pipeline.back();
	}	
}