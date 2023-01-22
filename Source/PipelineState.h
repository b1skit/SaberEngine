#pragma once


namespace gr
{
	struct PipelineState
	{
		enum class FaceCullingMode
		{
			Disabled,
			Front,
			Back,
			FrontBack,
			FaceCullingMode_Count
		} m_faceCullingMode = FaceCullingMode::Back;


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
		} m_srcBlendMode = BlendMode::One, m_dstBlendMode = BlendMode::One;


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
		} m_depthTestMode = DepthTestMode::GEqual;


		enum class DepthWriteMode
		{
			Enabled,
			Disabled,
			DepthWriteMode_Count
		} m_depthWriteMode = DepthWriteMode::Enabled;


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
		} m_colorWriteMode =
		{
			ColorWriteMode::ChannelMode::Enabled, // R
			ColorWriteMode::ChannelMode::Enabled, // G
			ColorWriteMode::ChannelMode::Enabled, // B
			ColorWriteMode::ChannelMode::Enabled  // A
		};


		enum class ClearTarget
		{
			Color,
			Depth,
			ColorDepth,
			None,
			ClearTarget_Count
		} m_targetClearMode = ClearTarget::None;


		struct
		{
			uint32_t m_targetFace = 0;
			uint32_t m_targetMip = 0;
		} m_textureTargetSetConfig;
	};
}