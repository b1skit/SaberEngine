// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "MeshPrimitive.h"


namespace gr::meshfactory
{
	extern std::shared_ptr<gr::MeshPrimitive> CreateCube();

	enum class ZLocation : uint8_t
	{
		Near,
		Far
	};
	extern std::shared_ptr<gr::MeshPrimitive> CreateFullscreenQuad(ZLocation zLocation);

	extern std::shared_ptr<gr::MeshPrimitive> CreateQuad(
		glm::vec3 tl /*= vec3(-0.5f, 0.5f, 0.0f)*/,
		glm::vec3 tr /*= vec3(0.5f, 0.5f, 0.0f)*/,
		glm::vec3 bl /*= vec3(-0.5f, -0.5f, 0.0f)*/,
		glm::vec3 br /*= vec3(0.5f, -0.5f, 0.0f)*/);

	extern std::shared_ptr<gr::MeshPrimitive> CreateSphere(
		float radius = 0.5f,
		size_t numLatSlices = 16,
		size_t numLongSlices = 16);

	// Creates a simple debug triangle.
	// Using the default arguments, the triangle will be in NDC.
	// Override the defaults to simulate a world-space transform (Reminder: We use a RHCS. Use negative zDepths to push
	// the triangle in front of the camera once a view-projection transformation is applied)
	extern std::shared_ptr<gr::MeshPrimitive> CreateHelloTriangle(float scale = 1.f, float zDepth = 0.5f);
}