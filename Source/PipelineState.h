// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "HashedDataObject.h"


namespace re
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
		enum class TopologyType : uint8_t
		{
			Point,
			Line,
			Triangle, // Default
			Patch
		};
		TopologyType GetTopologyType() const;
		void SetTopologyType(TopologyType);

		enum class FillMode
		{
			Wireframe, // TODO: Implement support for this
			Solid,  // Default
		};
		FillMode GetFillMode() const;
		void SetFillMode(FillMode);

		enum class FaceCullingMode
		{
			Disabled,
			Front,
			Back,  // Default
		};
		FaceCullingMode GetFaceCullingMode() const;
		void SetFaceCullingMode(FaceCullingMode);

		enum class WindingOrder // To determine a front-facing polygon
		{
			CCW,  // Default
			CW,
			WindingOrder_Count
		};
		WindingOrder GetWindingOrder() const;
		void SetWindingOrder(WindingOrder);

		enum class DepthTestMode
		{
			Less,		// < (Default)
			Never,		// Never pass
			Equal,		// ==
			LEqual,		// <=
			Greater,	// >
			NotEqual,	// !=
			GEqual,		// >=
			Always,		// Always pass: Disables depth testing
		};
		DepthTestMode GetDepthTestMode() const;
		void SetDepthTestMode(DepthTestMode);


	private:
		bool m_isDirty;

		TopologyType m_topologyType;
		FillMode m_fillMode;
		FaceCullingMode m_faceCullingMode;
		WindingOrder m_windingOrder;
		DepthTestMode m_depthTestMode;
	};
}