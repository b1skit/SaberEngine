// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "MeshPrimitive.h"


namespace gr::meshfactory
{
	struct FactoryOptions
	{
		bool m_generateNormalsAndTangents = false;
		
		bool m_generateVertexColors = false;
		glm::vec4 m_vertexColor = glm::vec4(1.f);
	};

	enum class ZLocation : uint8_t
	{
		Near,
		Far
	};

	
	extern std::shared_ptr<gr::MeshPrimitive> CreateCube(
		float extentDistance = 1.f, FactoryOptions const& factoryOptions = FactoryOptions{});

	extern std::shared_ptr<gr::MeshPrimitive> CreateFullscreenQuad(ZLocation zLocation);

	extern std::shared_ptr<gr::MeshPrimitive> CreateQuad(
		FactoryOptions const& factoryOptions = FactoryOptions{},
		glm::vec3 tl = glm::vec3(-0.5f, 0.5f, 0.0f),
		glm::vec3 tr = glm::vec3(0.5f, 0.5f, 0.0f),
		glm::vec3 bl = glm::vec3(-0.5f, -0.5f, 0.0f),
		glm::vec3 br = glm::vec3(0.5f, -0.5f, 0.0f));

	extern std::shared_ptr<gr::MeshPrimitive> CreateQuad(
		FactoryOptions const& factoryOptions = FactoryOptions{}, float extentDistance = 0.5f);

	extern std::shared_ptr<gr::MeshPrimitive> CreateSphere(
		FactoryOptions const& factoryOptions = FactoryOptions{},
		float radius = 0.5f,
		uint32_t numLatSlices = 16,
		uint32_t numLongSlices = 16);

	// Creates a simple debug triangle.
	// Using the default arguments, the triangle will be in NDC.
	// Override the defaults to simulate a world-space transform (Reminder: We use a RHCS. Use negative zDepths to push
	// the triangle in front of the camera once a view-projection transformation is applied)
	extern std::shared_ptr<gr::MeshPrimitive> CreateHelloTriangle(
		FactoryOptions const& factoryOptions = FactoryOptions{}, float scale = 1.f, float zDepth = 0.5);
}