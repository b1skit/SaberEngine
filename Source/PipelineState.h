// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "HashedDataObject.h"


namespace gr
{
	class PipelineState : public en::HashedDataObject
	{
	public:
		PipelineState();
		
		PipelineState(PipelineState const&) = default;
		PipelineState(PipelineState&&) = default;
		PipelineState& operator=(PipelineState const&) = default;
		~PipelineState() = default;
		

		uint64_t GetPipelineStateDataHash(); // Note: Use this instead of HashedDataObject::GetDataHash()

	private:
		void ComputeDataHash() override;
		

	public:
		enum class FillMode
		{
			Wireframe, // TODO: Implement support for this
			Solid,
			FillMode_Count
		};
		FillMode GetFillMode() const;
		void SetFillMode(FillMode);

		enum class FaceCullingMode
		{
			Disabled,
			Front,
			Back,
			FaceCullingMode_Count
		};
		FaceCullingMode GetFaceCullingMode() const;
		void SetFaceCullingMode(FaceCullingMode);

		enum class WindingOrder // To determine a front-facing polygon
		{
			CCW,
			CW,
			WindingOrder_Count
		};
		WindingOrder GetWindingOrder() const;
		void SetWindingOrder(WindingOrder);

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
		DepthTestMode GetDepthTestMode() const;
		void SetDepthTestMode(DepthTestMode);

		enum class DepthWriteMode
		{
			Enabled,
			Disabled
		};
		DepthWriteMode GetDepthWriteMode() const;
		void SetDepthWriteMode(DepthWriteMode);

		// TODO: Support blend operations (add/subtract/min/max etc) for both color and alpha channels

		// TODO: Support logical operations (AND/OR/XOR etc)

		// TODO: These should be per-target, to allow different outputs when using MRTs
		// TODO: We should support alpha blend modes, in addition to the color blend modes here
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
		BlendMode GetSrcBlendMode() const;
		void SetSrcBlendMode(BlendMode);
		BlendMode GetDstBlendMode() const;
		void SetDstBlendMode(BlendMode);

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
		};
		ColorWriteMode const& GetColorWriteMode() const;
		void SetColorWriteMode(ColorWriteMode const&);
		bool WritesColor() const;

		// TODO: These should be per-target, to allow different outputs when using MRTs
		enum class ClearTarget
		{
			Color,
			Depth,
			ColorDepth,
			None
		};
		ClearTarget GetClearTarget() const;
		void SetClearTarget(ClearTarget);


	private:
		bool m_isDirty;

		FillMode m_fillMode;
		FaceCullingMode m_faceCullingMode;
		WindingOrder m_windingOrder;
		DepthTestMode m_depthTestMode;
		DepthWriteMode m_depthWriteMode;
		BlendMode m_srcBlendMode;
		BlendMode m_dstBlendMode;
		ColorWriteMode m_colorWriteMode;
		bool m_writesColor;
		ClearTarget m_targetClearMode;
	};
}