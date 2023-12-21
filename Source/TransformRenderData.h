// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "RenderDataIDs.h"


namespace re
{
	class ParameterBlock;
}

namespace gr
{
	class Transform
	{
	public:
		struct RenderData
		{
			glm::mat4 g_model = glm::mat4(1.f); // Global TRS
			glm::mat4 g_transposeInvModel = glm::mat4(1.f);

			gr::TransformID m_transformID = gr::k_invalidTransformID;
		};


	public:
		struct InstancedTransformParams
		{
			glm::mat4 g_model;
			glm::mat4 g_transposeInvModel; // For constructing the normal map TBN matrix
			static constexpr char const* const s_shaderName = "InstancedTransformParams"; // Not counted towards size of struct
		};
		static std::shared_ptr<re::ParameterBlock> CreateInstancedTransformParams(glm::mat4 const* model, glm::mat4* transposeInvModel);
		static std::shared_ptr<re::ParameterBlock> CreateInstancedTransformParams(gr::Transform::RenderData const&);
		static std::shared_ptr<re::ParameterBlock> CreateInstancedTransformParams(std::vector<gr::Transform::RenderData const*> const&);
	};
}