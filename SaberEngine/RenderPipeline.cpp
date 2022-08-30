#pragma once

#include "RenderPipeline.h"

using gr::RenderStage;
using std::vector;

namespace gr
{
	vector<RenderStage const*>& RenderPipeline::AppendRenderStage(RenderStage const& renderStage)
	{
		// Append the render stage as a parent to a new vector of sequential render stages
		m_pipeline.emplace_back(vector<RenderStage const*>(1, { &renderStage }));

		// Return the new sequence of render stages, so child stages can be appended
		return m_pipeline.back();
	}
}