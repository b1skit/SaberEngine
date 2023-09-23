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
		

		uint64_t GetPipelineStateDataHash() const; // Note: Use this instead of HashedDataObject::GetDataHash()

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

		// TODO: These should be per-target (for each stage target set), to allow different behavior when using MRTs
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
		ClearTarget m_targetClearMode;
	};
}