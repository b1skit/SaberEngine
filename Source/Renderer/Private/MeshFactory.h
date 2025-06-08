// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Private/MeshPrimitive.h"

namespace core
{
	class Inventory;
}

namespace gr::meshfactory
{
	// Non-symmetric shapes are created with their highest point at (0, 0, 0), with the shape built in the -Y direction.
	// This can be overridden by post-rotating the generated verts
	enum Orientation : uint8_t
	{
		Default,	// Extending from (0,0,0) to -Y
		ZNegative,	// Towards -Z

		Orientation_Count
	};
	constexpr std::array<char const*, gr::meshfactory::Orientation::Orientation_Count> k_orientationNames =
	{
		"Default",
		"Z-Negative",
	};
	static_assert(k_orientationNames.size() == gr::meshfactory::Orientation::Orientation_Count);


	struct FactoryOptions
	{
		core::Inventory* m_inventory = nullptr;

		bool m_generateNormalsAndTangents = false;
		
		glm::vec4 m_vertexColor = glm::vec4(1.f); // GLTF default = (1,1,1,1)

		Orientation m_orientation = Orientation::Default;

		// If these are not null, they'll be populated with the min/max position values
		glm::vec3* m_positionMinXYZOut = nullptr;
		glm::vec3* m_positionMaxXYZOut = nullptr;
	};

	enum class ZLocation : uint8_t
	{
		Near,
		Far
	};

	
	extern core::InvPtr<gr::MeshPrimitive> CreateCube(
		FactoryOptions const& factoryOptions = FactoryOptions{}, float extentDistance = 1.f);

	extern core::InvPtr<gr::MeshPrimitive> CreateFullscreenQuad(core::Inventory*, ZLocation zLocation);

	extern core::InvPtr<gr::MeshPrimitive> CreateQuad(
		FactoryOptions const& factoryOptions = FactoryOptions{},
		glm::vec3 tl = glm::vec3(-0.5f, 0.5f, 0.0f),
		glm::vec3 tr = glm::vec3(0.5f, 0.5f, 0.0f),
		glm::vec3 bl = glm::vec3(-0.5f, -0.5f, 0.0f),
		glm::vec3 br = glm::vec3(0.5f, -0.5f, 0.0f));

	extern core::InvPtr<gr::MeshPrimitive> CreateQuad(
		FactoryOptions const& factoryOptions = FactoryOptions{}, float extentDistance = 0.5f);

	extern core::InvPtr<gr::MeshPrimitive> CreateSphere(
		FactoryOptions const& factoryOptions = FactoryOptions{},
		float radius = 0.5f,
		uint32_t numLatSlices = 16,
		uint32_t numLongSlices = 16);

	extern core::InvPtr<gr::MeshPrimitive> CreateCone(
		FactoryOptions const& factoryOptions = FactoryOptions{},
		float height = 1.f,
		float radius = 0.5f,
		uint32_t numSides = 16);

	extern core::InvPtr<gr::MeshPrimitive> CreateCylinder(
		FactoryOptions const& factoryOptions = FactoryOptions{},
		float height = 1.f,
		float radius = 0.5f,
		uint32_t numSides = 16);

	// Creates a simple debug triangle.
	// Using the default arguments, the triangle will be in NDC.
	// Override the defaults to simulate a world-space transform (Reminder: We use a RHCS. Use negative zDepths to push
	// the triangle in front of the camera once a view-projection transformation is applied)
	extern core::InvPtr<gr::MeshPrimitive> CreateHelloTriangle(
		FactoryOptions const& factoryOptions = FactoryOptions{}, float scale = 1.f, float zDepth = 0.5);
}