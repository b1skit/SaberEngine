// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "RenderStage.h"
#include "NamedObject.h"


namespace re
{
	class StagePipeline final : public virtual en::NamedObject
	{
	public:
		StagePipeline(std::string name) : NamedObject(name) {};

		StagePipeline(StagePipeline&&) = default;

		~StagePipeline() { Destroy(); };

		void Destroy();

		// TODO: This should include an iterator as an argument, to allow stages to be inserted at arbitrary locations
		std::vector<std::shared_ptr<re::RenderStage>>::iterator AppendRenderStage(std::shared_ptr<re::RenderStage>);
		std::vector<std::shared_ptr<re::RenderStage>>::iterator AppendSingleFrameRenderStage(std::shared_ptr<re::RenderStage>&&);

		size_t GetNumberOfStages() const { return m_renderStages.size(); }

		inline std::vector<std::shared_ptr<re::RenderStage>> const& GetRenderStages() const { return m_renderStages; }
		inline std::vector<std::shared_ptr<re::RenderStage>> const& GetSingleFrameRenderStages() const { return m_singleFrameRenderStages; }

		void EndOfFrame(); // Clear m_singleFrameStagePipeline etc

	private:
		std::vector<std::shared_ptr<re::RenderStage>> m_renderStages;
		std::vector<std::shared_ptr<re::RenderStage>> m_singleFrameRenderStages;

	private:
		StagePipeline() = delete;
		StagePipeline(StagePipeline const&) = delete;
		StagePipeline& operator=(StagePipeline const&) = delete;
	};


	class RenderPipeline final : public virtual en::NamedObject
	{
	public:	
		RenderPipeline(std::string const& name) : NamedObject(name) {}
		RenderPipeline(RenderPipeline&&) = default;
		RenderPipeline& operator=(RenderPipeline&&) = default;
		~RenderPipeline() { Destroy(); };

		void Destroy();

		StagePipeline& AddNewStagePipeline(std::string stagePipelineName);

		std::vector<StagePipeline>& GetStagePipeline() { return m_stagePipeline; }
		std::vector<StagePipeline> const& GetStagePipeline() const { return m_stagePipeline; }

		inline size_t GetNumberGraphicsSystems() const { return m_stagePipeline.size(); }


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
}