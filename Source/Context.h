// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "TextureTarget.h"


namespace re
{
	static constexpr char k_imguiIniPath[] = "config\\imgui.ini";


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
		};


	public:
		Context();
		
		std::shared_ptr<re::TextureTargetSet> GetBackbufferTextureTargetSet() const { return m_backbuffer; }

		Context::PlatformParams* const GetPlatformParams() { return m_platformParams.get(); }
		Context::PlatformParams const* const GetPlatformParams() const { return m_platformParams.get(); }
		void SetPlatformParams(std::unique_ptr<Context::PlatformParams> params) { m_platformParams = std::move(params); }

		// Platform wrappers:
		void Create();
		void Destroy();

		void Present() const;
		void SetVSyncMode(bool enabled) const;

		// Pipeline state:
		void SetCullingMode(Context::FaceCullingMode const& mode) const;
		void ClearTargets(Context::ClearTarget const& clearTarget) const;
		void SetBlendMode(Context::BlendMode const& src, Context::BlendMode const& dst) const;
		void SetDepthTestMode(Context::DepthTestMode const& mode) const;
		void SetDepthWriteMode(Context::DepthWriteMode const& mode) const;
		void SetColorWriteMode(Context::ColorWriteMode const& channelModes) const;
		
		// Static platform wrappers:
		static uint32_t GetMaxTextureInputs();		

	private:
		// Note: We store this as a shared_ptr so we can instantiate it once the context has been created
		std::shared_ptr<re::TextureTargetSet> m_backbuffer;
		
		std::unique_ptr<Context::PlatformParams> m_platformParams;
	};


	// We need to provide a destructor implementation since it's pure virtual
	inline Context::PlatformParams::~PlatformParams() {};
}