// � 2022 Adam Badke. All rights reserved.
#pragma once

#include "RenderStage.h"
#include "NamedObject.h"


namespace re
{
	class StagePipeline final : public virtual en::NamedObject
	{
	public:
		typedef std::list<std::shared_ptr<re::RenderStage>>::iterator StagePipelineItr;


	public:
		StagePipeline(std::string name) : NamedObject(name) {};

		StagePipeline(StagePipeline&&) = default;

		~StagePipeline() { Destroy(); };
		void Destroy();
	

		StagePipelineItr AppendRenderStage(std::shared_ptr<re::RenderStage>);
		StagePipelineItr AppendRenderStage(StagePipelineItr const& parent, std::shared_ptr<re::RenderStage>);

		StagePipelineItr AppendRenderStageForSingleFrame(StagePipelineItr const& parent, std::shared_ptr<re::RenderStage>);

		StagePipelineItr AppendSingleFrameRenderStage(std::shared_ptr<re::RenderStage>&&); // Append to end
		StagePipelineItr AppendSingleFrameRenderStage(StagePipelineItr const& parent, std::shared_ptr<re::RenderStage>&&);

		size_t GetNumberOfStages() const;

		std::list<std::shared_ptr<re::RenderStage>> const& GetRenderStages() const;

		void EndOfFrame(); // Calls RenderStage::EndOfFrame, clears single frame data etc


	private:
		std::list<std::shared_ptr<re::RenderStage>> m_renderStages;

		std::vector<StagePipelineItr> m_singleFrameInsertionPoints;


	private:
		StagePipeline() = delete;
		StagePipeline(StagePipeline const&) = delete;
		StagePipeline& operator=(StagePipeline const&) = delete;
	};


	class RenderPipeline final : public virtual en::NamedObject
	{
	public:	
		RenderPipeline(std::string const& name);

		RenderPipeline(RenderPipeline&&) = default;
		RenderPipeline& operator=(RenderPipeline&&) = default;

		~RenderPipeline() { Destroy(); };

		void Destroy();

		StagePipeline& AddNewStagePipeline(std::string const& stagePipelineName);

		std::vector<StagePipeline>& GetStagePipeline();
		std::vector<StagePipeline> const& GetStagePipeline() const;

		size_t GetNumberOfGraphicsSystems() const;


	private:
		// A 2D array: Columns processed in turn, left-to-right
		// *-*-*-*->
		// | | | |
		// * * * *
		//   |   |
		//   *   *
		//   |
		//   *
		std::vector<StagePipeline> m_stagePipeline;


	private:
		RenderPipeline() = delete;
		RenderPipeline(RenderPipeline const&) = delete;
		RenderPipeline& operator=(RenderPipeline const&) = delete;
	};


	inline size_t StagePipeline::GetNumberOfStages() const
	{
		return m_renderStages.size();
	}


	inline std::list<std::shared_ptr<re::RenderStage>> const& StagePipeline::GetRenderStages() const
	{
		return m_renderStages;
	}


	inline std::vector<StagePipeline>& RenderPipeline::GetStagePipeline()
	{
		return m_stagePipeline;
	}


	inline std::vector<StagePipeline> const& RenderPipeline::GetStagePipeline() const
	{
		return m_stagePipeline;
	}


	inline size_t RenderPipeline::GetNumberOfGraphicsSystems() const
	{
		return m_stagePipeline.size();
	}
}