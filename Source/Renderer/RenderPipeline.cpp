// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "RenderPipeline.h"

#include "Core/ProfilingMarkers.h"


namespace re
{
	/******************************************** StagePipeline********************************************/

	std::list<std::shared_ptr<re::Stage>>::iterator StagePipeline::AppendStage(
		std::shared_ptr<re::Stage> const& stage)
	{
		SEAssert(stage != nullptr, "Cannot append a null Stage");
		SEAssert(stage->GetStageLifetime() == re::Lifetime::Permanent,
			"Incorrect stage lifetime");
		
		m_stages.emplace_back(stage);
		return std::prev(m_stages.end());
	}


	StagePipeline::StagePipelineItr StagePipeline::AppendStage(
		StagePipeline::StagePipelineItr const& parentItr, 
		std::shared_ptr<re::Stage> const& stage)
	{
		SEAssert(stage != nullptr, "Cannot append a null Stage");
		SEAssert(stage->GetStageLifetime() == re::Lifetime::Permanent,
			"Incorrect stage lifetime");

		// std::list::emplace inserts the element directly before the iterator, so we advance to the next 
		const StagePipelineItr next = std::next(parentItr);

		StagePipelineItr newStageItr = m_stages.emplace(next, std::move(stage));

		return newStageItr;
	}


	StagePipeline::StagePipelineItr StagePipeline::AppendSingleFrameStage(
		std::shared_ptr<re::Stage>&& stage)
	{
		SEAssert(stage != nullptr, "Cannot append a null Stage");
		SEAssert(stage->GetStageLifetime() == re::Lifetime::SingleFrame,
			"Incorrect stage lifetime");

		m_stages.emplace_back(std::move(stage));

		const StagePipelineItr lastItem = std::prev(m_stages.end());

		m_singleFrameInsertionPoints.emplace_back(lastItem);

		return lastItem;
	}


	StagePipeline::StagePipelineItr StagePipeline::AppendSingleFrameStage(
		StagePipeline::StagePipelineItr const& parentItr,
		std::shared_ptr<re::Stage>&& stage)
	{
		SEAssert(stage != nullptr, "Cannot append a null Stage");
		SEAssert(stage->GetStageLifetime() == re::Lifetime::SingleFrame,
			"Incorrect stage lifetime");
		
		// std::list::emplace inserts the element directly before the iterator, so we advance to the next 
		const StagePipelineItr next = std::next(parentItr);
		
		StagePipelineItr newSingleFrameStageItr;
		if (next == m_stages.end())
		{
			m_stages.emplace_back(std::move(stage));
			newSingleFrameStageItr = std::prev(m_stages.end());
		}
		else
		{
			newSingleFrameStageItr = m_stages.emplace(next, std::move(stage));
		}

		m_singleFrameInsertionPoints.emplace_back(newSingleFrameStageItr);

		return newSingleFrameStageItr;
	}


	StagePipeline::StagePipelineItr StagePipeline::AppendStageForSingleFrame(
		StagePipeline::StagePipelineItr const& parentItr,
		std::shared_ptr<re::Stage> const& stage)
	{
		SEAssert(stage != nullptr, "Cannot append a null Stage");
		SEAssert(stage->GetStageLifetime() == re::Lifetime::Permanent,
			"Incorrect stage lifetime");

		// std::list::emplace inserts the element directly before the iterator, so we advance to the next 
		const StagePipelineItr next = std::next(parentItr);

		StagePipelineItr newSingleFrameStageItr;
		if (next == m_stages.end())
		{
			m_stages.emplace_back(stage);
			newSingleFrameStageItr = std::prev(m_stages.end());
		}
		else
		{
			newSingleFrameStageItr = m_stages.emplace(next, stage);
		}

		m_singleFrameInsertionPoints.emplace_back(newSingleFrameStageItr);

		return newSingleFrameStageItr;
	}


	void StagePipeline::PostUpdatePreRender()
	{
		SEBeginCPUEvent("StagePipeline::PostUpdatePreRender");

		for (std::shared_ptr<re::Stage>& stage : m_stages)
		{
			stage->PostUpdatePreRender();
		}

		SEEndCPUEvent();
	}


	void StagePipeline::EndOfFrame()
	{
		SEBeginCPUEvent("StagePipeline::EndOfFrame");

		for (std::shared_ptr<re::Stage>& stage : m_stages)
		{
			stage->EndOfFrame();
		}

		// Other references/iterators are not affected when we erase std::list elements
		for (auto const& insertionPoint : m_singleFrameInsertionPoints)
		{
			m_stages.erase(insertionPoint);
		}
		m_singleFrameInsertionPoints.clear();

		SEEndCPUEvent();
	}


	void StagePipeline::Destroy()
	{
		m_stages.clear();
		m_singleFrameInsertionPoints.clear();
	}


	/******************************************** RenderPipeline********************************************/
	
	constexpr size_t k_numReservedStages = 32;


	RenderPipeline::RenderPipeline(std::string const& name)
		: INamedObject(name)
	{
		m_stagePipeline.reserve(k_numReservedStages);
	}


	StagePipeline& RenderPipeline::AddNewStagePipeline(std::string const& stagePipelineName)
	{ 
		m_stagePipeline.emplace_back(stagePipelineName);

		SEAssert(m_stagePipeline.size() <= k_numReservedStages,
			"m_stagePipeline was resized, this invalidates all pointers/iterators. Increase k_numReservedStages");

		return m_stagePipeline.back();
	}


	void RenderPipeline::Destroy()
	{
		m_stagePipeline.clear();
	}


	void RenderPipeline::PostUpdatePreRender()
	{
		SEBeginCPUEvent(std::format("{} RenderPipeline::PostUpdatePreRender", GetName().c_str()).c_str());

		for (StagePipeline& stagePipeline : m_stagePipeline)
		{
			stagePipeline.PostUpdatePreRender();
		}

		SEEndCPUEvent();
	}


	void RenderPipeline::EndOfFrame()
	{
		SEBeginCPUEvent(std::format("{} RenderPipeline::EndOfFrame", GetName().c_str()).c_str());

		for (StagePipeline& stagePipeline : m_stagePipeline)
		{
			stagePipeline.EndOfFrame();
		}

		SEEndCPUEvent();
	}
}