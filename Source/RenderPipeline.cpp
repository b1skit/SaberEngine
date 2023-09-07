// � 2022 Adam Badke. All rights reserved.
#pragma once

#include "RenderPipeline.h"

using re::RenderStage;


namespace re
{
	/******************************************** StagePipeline********************************************/

	std::list<std::shared_ptr<re::RenderStage>>::iterator StagePipeline::AppendRenderStage(
		std::shared_ptr<re::RenderStage> renderStage)
	{
		SEAssert("Cannot append a null RenderStage", renderStage != nullptr);
		SEAssert("Incorrect stage lifetime",
			renderStage->GetStageLifetime() == re::RenderStage::RenderStageLifetime::Permanent);
		
		m_renderStages.emplace_back(renderStage);
		return std::prev(m_renderStages.end());
	}


	StagePipeline::StagePipelineItr StagePipeline::AppendRenderStage(
		StagePipeline::StagePipelineItr const& parentItr, 
		std::shared_ptr<re::RenderStage> renderStage)
	{
		SEAssert("Cannot append a null RenderStage", renderStage != nullptr);
		SEAssert("Incorrect stage lifetime",
			renderStage->GetStageLifetime() == re::RenderStage::RenderStageLifetime::Permanent);

		// std::list::emplace inserts the element directly before the iterator, so we advance to the next 
		const StagePipelineItr next = std::next(parentItr);

		StagePipelineItr newStageItr = m_renderStages.emplace(next, std::move(renderStage));

		return newStageItr;
	}


	StagePipeline::StagePipelineItr StagePipeline::AppendSingleFrameRenderStage(
		std::shared_ptr<re::RenderStage>&& renderStage)
	{
		SEAssert("Cannot append a null RenderStage", renderStage != nullptr);
		SEAssert("Incorrect stage lifetime",
			renderStage->GetStageLifetime() == re::RenderStage::RenderStageLifetime::SingleFrame);

		m_renderStages.emplace_back(std::move(renderStage));

		const StagePipelineItr lastItem = std::prev(m_renderStages.end());

		m_singleFrameInsertionPoints.emplace_back(lastItem);

		return lastItem;
	}


	StagePipeline::StagePipelineItr StagePipeline::AppendSingleFrameRenderStage(
		StagePipeline::StagePipelineItr const& parentItr,
		std::shared_ptr<re::RenderStage>&& renderStage)
	{
		SEAssert("Cannot append a null RenderStage", renderStage != nullptr);
		SEAssert("Incorrect stage lifetime", 
			renderStage->GetStageLifetime() == re::RenderStage::RenderStageLifetime::SingleFrame);
		
		// std::list::emplace inserts the element directly before the iterator, so we advance to the next 
		const StagePipelineItr next = std::next(parentItr);
		 
		StagePipelineItr newSingleFrameStageItr = m_renderStages.emplace(next, std::move(renderStage));

		m_singleFrameInsertionPoints.emplace_back(newSingleFrameStageItr);

		return newSingleFrameStageItr;
	}


	void StagePipeline::EndOfFrame()
	{
		for (std::shared_ptr<re::RenderStage> renderStage : m_renderStages)
		{
			renderStage->EndOfFrame();
		}

		// Other references/iterators are not affected when we erase std::list elements
		for (auto const& insertionPoint : m_singleFrameInsertionPoints)
		{
			m_renderStages.erase(insertionPoint);
		}
		m_singleFrameInsertionPoints.clear();
	}


	void StagePipeline::Destroy()
	{
		m_renderStages.clear();
		m_singleFrameInsertionPoints.clear();
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