#pragma once

#include "RenderPipeline.h"

using re::RenderStage;
using std::vector;

namespace
{
	inline void SanityCheckRenderStage(RenderStage const& renderStage)
	{
		SEAssert("renderStage not fully configured",
			renderStage.GetName() != "" &&
			renderStage.GetStageShader() != nullptr
			// TODO: Add more conditions
		);
		// Note: Null stage geometry, camera allowed
	}
}

namespace re
{
	/******************************************** StagePipeline********************************************/

	std::vector<re::RenderStage*>::iterator StagePipeline::AppendRenderStage(RenderStage& renderStage)
	{
		SanityCheckRenderStage(renderStage);
		
		m_stagePipeline.emplace_back(&renderStage);
		return m_stagePipeline.end();
	}


	std::vector<re::RenderStage>::iterator StagePipeline::AppendSingleFrameRenderStage(RenderStage& renderStage)
	{
		SanityCheckRenderStage(renderStage);
		
		m_singleFrameStagePipeline.emplace_back(renderStage);
		return m_singleFrameStagePipeline.end();
	}


	void StagePipeline::EndOfFrame()
	{
		m_singleFrameStagePipeline.clear();
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