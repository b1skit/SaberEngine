#pragma once

#include "RenderPipeline.h"

using gr::RenderStage;
using std::vector;

namespace re
{
	/******************************************** StagePipeline********************************************/

	std::vector<gr::RenderStage const*>::iterator StagePipeline::AppendRenderStage(RenderStage const& renderStage)
	{
		SEAssert("renderStage not fully configured",
			renderStage.GetName() != "" &&
			renderStage.GetStageShader() != nullptr &&
			renderStage.GetStageCamera() != nullptr
			// TODO: Add more conditions
		);

		m_stagePipeline.emplace_back(&renderStage);
		return m_stagePipeline.end();
	}


	std::vector<gr::RenderStage>::iterator StagePipeline::AppendSingleFrameRenderStage(RenderStage const& renderStage)
	{
		SEAssert("renderStage not fully configured",
			renderStage.GetName() != "" &&
			renderStage.GetStageShader() != nullptr &&
			renderStage.GetStageCamera() != nullptr
			// TODO: Add more conditions
		);
		
		m_singleFrameStagePipeline.emplace_back(renderStage);
		return m_singleFrameStagePipeline.end();
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