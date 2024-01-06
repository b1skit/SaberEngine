// � 2023 Adam Badke. All rights reserved.
#pragma once
#include "RenderObjectIDs.h"


namespace re
{
	class ParameterBlock;
}

namespace gr
{
	class Transform
	{
	public:
		// Static world-space CS axis (SaberEngine currently uses a RHCS)
		static const glm::vec3 WorldAxisX;	// +X
		static const glm::vec3 WorldAxisY;	// +Y
		static const glm::vec3 WorldAxisZ;	// +Z


	public:
		struct RenderData
		{
			glm::mat4 g_model = glm::mat4(1.f); // Global TRS
			glm::mat4 g_transposeInvModel = glm::mat4(1.f);

			glm::vec3 m_globalPosition; // World-space position
			glm::vec3 m_globalScale;

			glm::vec3 m_globalRight; // World-space right (X+) vector
			glm::vec3 m_globalUp; // World-space up (Y+) vector
			glm::vec3 m_globalForward; // World-space forward (Z+) vector

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