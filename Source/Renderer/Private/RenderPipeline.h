// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Private/Stage.h"

#include "Core/Interfaces/INamedObject.h"


namespace re
{
	class StagePipeline final : public virtual core::INamedObject
	{
	public:
		typedef std::list<std::shared_ptr<re::Stage>>::iterator StagePipelineItr;


	public:
		StagePipeline(std::string name) : INamedObject(name) {};

		StagePipeline(StagePipeline&&) noexcept = default;
		StagePipeline& operator=(StagePipeline&&) noexcept = default;

		~StagePipeline() { Destroy(); };
		void Destroy();
	

		StagePipelineItr AppendStage(std::shared_ptr<re::Stage> const&);
		StagePipelineItr AppendStage(StagePipelineItr const& parent, std::shared_ptr<re::Stage> const&);

		StagePipelineItr AppendStageForSingleFrame(StagePipelineItr const& parent, std::shared_ptr<re::Stage> const&);

		StagePipelineItr AppendSingleFrameStage(std::shared_ptr<re::Stage>&&); // Append to end
		StagePipelineItr AppendSingleFrameStage(StagePipelineItr const& parent, std::shared_ptr<re::Stage>&&);

		size_t GetNumberOfStages() const;

		std::list<std::shared_ptr<re::Stage>> const& GetStages() const;


	public:
		void PostUpdatePreRender();
		void EndOfFrame(); // Calls Stage::EndOfFrame, clears single frame data etc


	private:
		std::list<std::shared_ptr<re::Stage>> m_stages;

		std::vector<StagePipelineItr> m_singleFrameInsertionPoints;


	private:
		StagePipeline() = delete;
		StagePipeline(StagePipeline const&) = delete;
		StagePipeline& operator=(StagePipeline const&) = delete;
	};


	class RenderPipeline final : public virtual core::INamedObject
	{
	public:	
		RenderPipeline(std::string const& name);

		RenderPipeline(RenderPipeline&&) noexcept = default;
		RenderPipeline& operator=(RenderPipeline&&) noexcept = default;

		~RenderPipeline() { Destroy(); };

		void Destroy();


	public:
		void PostUpdatePreRender();
		void EndOfFrame();


	public:
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
		return m_stages.size();
	}


	inline std::list<std::shared_ptr<re::Stage>> const& StagePipeline::GetStages() const
	{
		return m_stages;
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