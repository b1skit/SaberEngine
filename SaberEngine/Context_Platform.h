#pragma once

#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>


namespace re
{
	class Context;
}

namespace platform
{
	class Context
	{
	public:
		struct PlatformParams
		{
			PlatformParams() = default;
			PlatformParams(PlatformParams const&) = delete;
			virtual ~PlatformParams() = 0;

			const glm::vec4 m_windowClearColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
			const float m_depthClearColor = 1.0f;


			// API-specific function pointers:
			static void CreatePlatformParams(re::Context& context);
		};


	public:


		// Static function pointers:
		static void (*Create)(re::Context& context);
		static void (*Destroy)(re::Context& context);
		static void (*SwapWindow)(re::Context& context);

	private:

	};

	// We need to provide a destructor implementation since it's pure virutal
	inline Context::PlatformParams::~PlatformParams() {};
}