// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Core/Interfaces/IHashedDataObject.h"


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
		// High-level primitive topology type used to configure the PSO. 
		// Any similar gr::MeshPrimitive::PrimitiveTopology elements can be used interchangeably with a PSO with a 
		// matching PrimitiveTopologyType. E.g. PrimitiveTopology::Line* -> PrimitiveTopologyType::Line
		enum class PrimitiveTopologyType : uint8_t
		{
			Triangle, // Default
			Point,
			Line,
			Patch
		};
		static PrimitiveTopologyType CStrToPrimitiveTopologyType(char const*);

		PrimitiveTopologyType GetPrimitiveTopologyType() const;
		void SetPrimitiveTopologyType(PrimitiveTopologyType);
		

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
		PrimitiveTopologyType m_primitiveTopologyType;
		FillMode m_fillMode;
		FaceCullingMode m_faceCullingMode;
		WindingOrder m_windingOrder;
		DepthTestMode m_depthTestMode;
	};
}