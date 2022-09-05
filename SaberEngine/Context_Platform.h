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
		enum class FaceCullingMode
		{
			Disabled,
			Front,
			Back,
			FrontBack,
			FaceCullingMode_Count
		};

		enum class ClearTarget
		{
			Color,
			Depth,
			ColorDepth,
			None,
			ClearTarget_Count
		};

		enum class BlendMode
		{
			Disabled,
			Default, // Src one, Dst zero
			Zero,
			One,
			SrcColor,
			OneMinusSrcColor,
			DstColor,
			OneMinusDstColor,
			SrcAlpha,
			OneMinusSrcAlpha,
			DstAlpha,
			OneMinusDstAlpha,
			BlendMode_Count
		};

		enum class DepthMode
		{
			Default,	// Less
			Never,
			Less,		// <
			Equal,		// ==
			LEqual,		// <=
			Greater,	// >
			NotEqual,	// !=
			GEqual,		// >=
			Always,
			DepthMode_Count
		};

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
		static void (*SwapWindow)(re::Context const& context);
		static void (*SetCullingMode)(FaceCullingMode const& mode);
		static void (*ClearTargets)(ClearTarget const& clearTarget);
		static void (*SetBlendMode)(BlendMode const& src, BlendMode const& dst);
		static void(*SetDepthMode)(DepthMode const& mode);
		static uint32_t(*GetMaxTextureInputs)();

	private:

	};

	// We need to provide a destructor implementation since it's pure virtual
	inline Context::PlatformParams::~PlatformParams() {};
}