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

		std::vector<re::RenderStage*>::iterator AppendRenderStage(re::RenderStage* renderStage);
		std::vector<re::RenderStage>::iterator AppendSingleFrameRenderStage(re::RenderStage const& renderStage);

		size_t GetNumberOfStages() const { return m_stagePipeline.size(); }

		inline std::vector<re::RenderStage*>& GetRenderStages() { return m_stagePipeline; }
		inline std::vector<re::RenderStage*> const& GetRenderStages() const { return m_stagePipeline; }

		inline std::vector<re::RenderStage>& GetSingleFrameRenderStages() { return m_singleFrameStagePipeline; }
		inline std::vector<re::RenderStage> const& GetSingleFrameRenderStages() const { return m_singleFrameStagePipeline; }

		void EndOfFrame(); // Clear m_singleFrameStagePipeline etc

	private:
		std::vector<re::RenderStage*> m_stagePipeline;
		std::vector<re::RenderStage> m_singleFrameStagePipeline;

	private:
		StagePipeline() = delete;
		StagePipeline(StagePipeline const&) = delete;
		StagePipeline& operator=(StagePipeline const&) = delete;
	};


	class RenderPipeline final : public virtual en::NamedObject
	{
	public:	
		RenderPipeline(std::string const& name) : NamedObject(name) {}
		~RenderPipeline() { Destroy(); };

		void Destroy();

		StagePipeline& AddNewStagePipeline(std::string stagePipelineName);

		std::vector<StagePipeline>& GetPipeline() { return m_pipeline; }
		std::vector<StagePipeline> const& GetPipeline() const { return m_pipeline; }

		inline size_t GetNumberGraphicsSystems() const { return m_pipeline.size(); }


	private:
		// A 2D array: Columns processed in turn, left-to-right
		// *-*-*-*->
		// | | | |
		// * * * *
		//   |   |
		//   *   *
		//   |
		//   *
		std::vector<StagePipeline> m_pipeline;


	private:
		RenderPipeline() = delete;
		RenderPipeline(RenderPipeline const&) = delete;
		RenderPipeline(RenderPipeline&&) = delete;
		RenderPipeline& operator=(RenderPipeline const&) = delete;
	};
}