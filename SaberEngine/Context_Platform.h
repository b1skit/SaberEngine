// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace re
{
	class Context;
}


namespace platform
{
	class Context
	{
		// TODO: These enums should belong to re::Context. The platform layer is just a common binding interface.
		// We currently define them here to avoid a cyclic dependency: Context.h includes Context_Platform for 
		// Context_Platform::PlatformParams. We need to move the PlatformParams to re::Context, and inherit from it
		// on the API layer

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

		enum class DepthTestMode
		{
			Default,	// Less
			Never,		// Never pass
			Less,		// <
			Equal,		// ==
			LEqual,		// <=
			Greater,	// >
			NotEqual,	// !=
			GEqual,		// >=
			Always,		// Always pass: Disables depth testing
			DepthTestMode_Count
		};

		enum class DepthWriteMode
		{
			Enabled,
			Disabled,
			DepthWriteMode_Count
		};

		
		struct ColorWriteMode
		{
			enum class ChannelMode
			{
				Enabled,
				Disabled,
				ChannelMode_Count
			};
			ChannelMode R = ChannelMode::Enabled;
			ChannelMode G = ChannelMode::Enabled;
			ChannelMode B = ChannelMode::Enabled;
			ChannelMode A = ChannelMode::Enabled;
		};

	public:
		struct PlatformParams
		{
			PlatformParams() = default;
			PlatformParams(PlatformParams const&) = delete;
			virtual ~PlatformParams() = 0;

			// API-specific function pointers:
			static void CreatePlatformParams(re::Context& m_context);
		};


	public:
		// Static function pointers:
		static void (*Create)(re::Context& context);
		static void (*Destroy)(re::Context& context);
		static void (*Present)(re::Context const& context);
		static void (*SetVSyncMode)(re::Context const& window, bool enabled);
		static void (*SetCullingMode)(FaceCullingMode const& mode);
		static void (*ClearTargets)(ClearTarget const& clearTarget);
		static void (*SetBlendMode)(BlendMode const& src, BlendMode const& dst);
		static void (*SetDepthTestMode)(DepthTestMode const& mode);
		static void (*SetDepthWriteMode)(DepthWriteMode const& mode);
		static void (*SetColorWriteMode)(ColorWriteMode const& channelModes);
		static uint32_t(*GetMaxTextureInputs)();
	};

	// We need to provide a destructor implementation since it's pure virtual
	inline Context::PlatformParams::~PlatformParams() {};
}