#pragma once

#include <vector>

#include "RenderStage.h"


namespace re
{
	class StagePipeline
	{
	public:
		StagePipeline(std::string name) : m_name(name) {};
		
		~StagePipeline() = default;
		StagePipeline(StagePipeline&&) = default;

		StagePipeline() = delete;
		StagePipeline(StagePipeline const&) = delete;
		StagePipeline& operator=(StagePipeline const&) = delete;

		std::vector<gr::RenderStage const*>::iterator AppendRenderStage(gr::RenderStage const& renderStage);

		inline std::string const& GetName() { return m_name; }
		size_t GetNumberOfStages() const { return m_stagePipeline.size(); }

		gr::RenderStage const* operator[](size_t index) { return m_stagePipeline[index]; }

	private:
		std::string m_name;
		std::vector<gr::RenderStage const*> m_stagePipeline;
	};


	class RenderPipeline
	{
	public:
		RenderPipeline() = default;
		~RenderPipeline() = default;

		RenderPipeline(RenderPipeline const&) = delete;
		RenderPipeline(RenderPipeline&&) = delete;
		RenderPipeline& operator=(RenderPipeline const&) = delete;

		StagePipeline& AddNewStagePipeline(std::string stagePipelineName);

		std::vector<StagePipeline>& GetPipeline() { return m_pipeline; }
		std::vector<StagePipeline> const& GetPipeline() const { return m_pipeline; }

		inline size_t GetNumberGraphicsSystems() const { return m_pipeline.size(); }
		inline size_t GetNumberOfGraphicsSystemStages(size_t gsIndex) const { return m_pipeline[gsIndex].GetNumberOfStages(); }


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
	};
}