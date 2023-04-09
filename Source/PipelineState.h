#pragma once


namespace gr
{
	struct PipelineState
	{
		enum class FillMode
		{
			Wireframe, // TODO: Implement support for this
			Solid,
			FillMode_Count
		} m_fillMode = FillMode::Solid;


		enum class FaceCullingMode
		{
			Disabled,
			Front,
			Back,
			FaceCullingMode_Count
		} m_faceCullingMode = FaceCullingMode::Back;


		enum class WindingOrder // To determine a front-facing polygon
		{
			CCW,
			CW,
			WindingOrder_Count
		} m_windingOrder = WindingOrder::CCW;


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
		} m_depthTestMode = DepthTestMode::Default;


		enum class DepthWriteMode
		{
			Enabled,
			Disabled,
			DepthWriteMode_Count
		} m_depthWriteMode = DepthWriteMode::Enabled;


		// TODO: These should be per-target, to allow different outputs when using MRTs
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
		// TODO: We should support alpha blend modes, in addition to the color blend modes here


		// TODO: Support blend operations (add/subtract/min/max etc) for both color and alpha channels

		// TODO: Support logical operations (AND/OR/XOR etc)


		// TODO: These should be per-target, to allow different outputs when using MRTs
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


		// TODO: These should be per-target, to allow different outputs when using MRTs
		enum class ClearTarget
		{
			Color,
			Depth,
			ColorDepth,
			None,
			ClearTarget_Count
		} m_targetClearMode = ClearTarget::None;


		// TODO: We should be able to target individual target sub-resources, instead of specifying this here
		struct
		{
			uint32_t m_targetFace = 0;
			uint32_t m_targetMip = 0;
		} m_textureTargetSetConfig;
	};
}