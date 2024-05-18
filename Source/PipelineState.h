// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Core\Interfaces\IHashedDataObject.h"


namespace re
{
	class PipelineState : public core::IHashedDataObject
	{
	public:
		PipelineState();
		
		PipelineState(PipelineState const&) = default;
		PipelineState(PipelineState&&) = default;
		PipelineState& operator=(PipelineState const&) = default;
		~PipelineState() = default;
		

		uint64_t GetPipelineStateDataHash() const; // Note: Use this instead of IHashedDataObject::GetDataHash()

	private:
		void ComputeDataHash() override;
		

	public:
		enum class TopologyType : uint8_t
		{
			Triangle, // Default
			Point,
			Line,
			Patch
		};
		TopologyType GetTopologyType() const;
		void SetTopologyType(TopologyType);
		static TopologyType GetTopologyTypeByName(char const*);

		enum class FillMode : uint8_t
		{
			Solid, // Default
			Wireframe
			// Note: Point fill modes are not supported, even if an API supports them
		};
		FillMode GetFillMode() const;
		void SetFillMode(FillMode);
		static FillMode GetFillModeByName(char const*);

		enum class FaceCullingMode
		{
			Back,  // Default
			Front,
			Disabled,
		};
		FaceCullingMode GetFaceCullingMode() const;
		void SetFaceCullingMode(FaceCullingMode);
		static FaceCullingMode GetFaceCullingModeByName(char const*);

		enum class WindingOrder // To determine a front-facing polygon
		{
			CCW,  // Default
			CW,
		};
		WindingOrder GetWindingOrder() const;
		void SetWindingOrder(WindingOrder);
		static WindingOrder GetWindingOrderByName(char const*);

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
		static DepthTestMode GetDepthTestModeByName(char const*);


	private:
		bool m_isDirty;

		// Initialized in ctor
		TopologyType m_topologyType;
		FillMode m_fillMode;
		FaceCullingMode m_faceCullingMode;
		WindingOrder m_windingOrder;
		DepthTestMode m_depthTestMode;
	};
}