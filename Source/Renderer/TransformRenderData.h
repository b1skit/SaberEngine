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
		static InstancedTransformData CreateInstancedTransformData(gr::Transform::RenderData const&);

		static InstancedTransformData CreateInstancedTransformData(
			glm::mat4 const* model, glm::mat4 const* transposeInvModel);

		static re::BufferInput CreateInstancedTransformBuffer(
			re::Buffer::AllocationType, glm::mat4 const* model, glm::mat4* transposeInvModel);
		static re::BufferInput CreateInstancedTransformBuffer(
			re::Buffer::AllocationType, gr::Transform::RenderData const&);
		static re::BufferInput CreateInstancedTransformBuffer(
			re::Buffer::AllocationType, std::vector<gr::Transform::RenderData const*> const&);
	};
}