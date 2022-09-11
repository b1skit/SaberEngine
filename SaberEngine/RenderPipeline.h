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

		inline std::string const& GetName() const { return m_name; }

		std::vector<gr::RenderStage const*>::iterator AppendRenderStage(gr::RenderStage const& renderStage);
		std::vector<gr::RenderStage>::iterator AppendSingleFrameRenderStage(gr::RenderStage const& renderStage);

		size_t GetNumberOfStages() const { return m_stagePipeline.size(); }

		inline std::vector<gr::RenderStage const*>& GetRenderStages() { return m_stagePipeline; }
		inline std::vector<gr::RenderStage const*> const& GetRenderStages() const { return m_stagePipeline; }

		inline std::vector<gr::RenderStage>& GetSingleFrameRenderStages() { return m_singleFrameStagePipeline; }
		inline std::vector<gr::RenderStage> const& GetSingleFrameRenderStages() const { return m_singleFrameStagePipeline; }

		void EndOfFrame(); // Clear m_singleFrameStagePipeline etc

	private:
		std::string const m_name;
		std::vector<gr::RenderStage const*> m_stagePipeline;
		std::vector<gr::RenderStage> m_singleFrameStagePipeline;
	};


	class RenderPipeline
	{
	public:	
		RenderPipeline(std::string const& name) : m_name(name) {}
		~RenderPipeline() = default;

		RenderPipeline() = delete;
		RenderPipeline(RenderPipeline const&) = delete;
		RenderPipeline(RenderPipeline&&) = delete;
		RenderPipeline& operator=(RenderPipeline const&) = delete;

		StagePipeline& AddNewStagePipeline(std::string stagePipelineName);

		std::vector<StagePipeline>& GetPipeline() { return m_pipeline; }
		std::vector<StagePipeline> const& GetPipeline() const { return m_pipeline; }

		inline size_t GetNumberGraphicsSystems() const { return m_pipeline.size(); }
		/*inline size_t GetNumberOfGraphicsSystemStages(size_t gsIndex) const 
			{ return m_pipeline[gsIndex].GetNumberOfStages(); }*/


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

		std::string const m_name;
	};
}