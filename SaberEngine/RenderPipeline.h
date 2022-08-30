#pragma once

#include <vector>

#include "RenderStage.h"


namespace gr
{
	class RenderPipeline
	{
	public:
		std::vector<gr::RenderStage const*>& AppendRenderStage(gr::RenderStage const& renderStage);

		inline std::vector<std::vector<gr::RenderStage const*>>& GetPipeline() { return m_pipeline; }
		inline std::vector<std::vector<gr::RenderStage const*>> const& GetPipeline() const { return m_pipeline; }

	private:
		// A 2D array: Columns processed in turn, left-to-right
		// *-*-*-*->
		// | | | |
		// * * * *
		//   |   |
		//   *   
		//   |
		//   *
		std::vector<std::vector<gr::RenderStage const*>> m_pipeline;
	};
}