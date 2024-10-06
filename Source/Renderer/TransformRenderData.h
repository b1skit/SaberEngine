// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Buffer.h"
#include "BufferInput.h"
#include "RenderObjectIDs.h"

#include "Shaders/Common/InstancingParams.h"


namespace gr
{
	class Transform
	{
	public:
		// Static world-space CS axis (SaberEngine currently uses a RHCS)
		static constexpr glm::vec3 WorldAxisX = glm::vec3(1.0f, 0.0f, 0.0f);	// +X
		static constexpr glm::vec3 WorldAxisY = glm::vec3(0.0f, 1.0f, 0.0f);	// +Y
		static constexpr glm::vec3 WorldAxisZ = glm::vec3(0.0f, 0.0f, 1.0f);	// +Z


	public:
		struct RenderData
		{
			glm::mat4 g_model = glm::mat4(1.f); // Global TRS
			glm::mat4 g_transposeInvModel = glm::mat4(1.f);

			glm::vec3 m_globalPosition = glm::vec3(0.f); // World-space position
			glm::vec3 m_globalScale = glm::vec3(1.f);

			glm::vec3 m_globalRight = WorldAxisX; // World-space right (X+) vector
			glm::vec3 m_globalUp = WorldAxisY; // World-space up (Y+) vector
			glm::vec3 m_globalForward = WorldAxisZ; // World-space forward (Z+) vector

			gr::TransformID m_transformID = gr::k_invalidTransformID;
		};


	public:
		static InstancedTransformData CreateInstancedTransformData(gr::Transform::RenderData const&);

		static InstancedTransformData CreateInstancedTransformData(
			glm::mat4 const* model, glm::mat4 const* transposeInvModel);

		static re::BufferInput CreateInstancedTransformBuffer(
			re::Lifetime, re::Buffer::StagingPool, glm::mat4 const* model, glm::mat4* transposeInvModel);
		static re::BufferInput CreateInstancedTransformBuffer(
			re::Lifetime, re::Buffer::StagingPool, gr::Transform::RenderData const&);
		static re::BufferInput CreateInstancedTransformBuffer(
			re::Lifetime, re::Buffer::StagingPool, std::vector<gr::Transform::RenderData const*> const&);
	};
}