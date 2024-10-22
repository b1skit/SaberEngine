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
		PipelineState(PipelineState&&) noexcept = default;
		PipelineState& operator=(PipelineState const&) = default;
		PipelineState& operator=(PipelineState&&) = default;
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
		

		// Rasterizer state:
		//------------------
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

		int GetDepthBias() const;
		void SetDepthBias(int);

		float GetDepthBiasClamp() const;
		void SetDepthBiasClamp(float);

		float GetSlopeScaledDepthBias() const;
		void SetSlopeScaledDepthBias(float);

		bool GetDepthClipEnabled() const;
		void SetDepthClipEnabled(bool);

		bool GetMultiSampleEnabled() const;
		void SetMultiSampleEnabled(bool);

		bool GetAntiAliasedLineEnabled() const;
		void SetAntiAliasedLineEnabled(bool);

		uint8_t GetForcedSampleCount() const;
		void SetForcedSampleCount(uint8_t);

		bool GetConservativeRaster() const;
		void SetConservativeRaster(bool);


		// Depth stencil state:
		//---------------------
		bool GetDepthTestEnabled() const;
		void SetDepthTestEnabled(bool);

		enum class DepthWriteMask
		{
			Zero,	// Turn off writes to the depth-stencil buffer
			All,	// Turn on writes to the depth-stencil buffer
		};
		DepthWriteMask GetDepthWriteMask() const;
		void SetDepthWriteMask(DepthWriteMask);
		static DepthWriteMask GetDepthWriteMaskByName(char const*);

		enum class ComparisonFunc
		{
			Less,		// < (Default)
			Never,		// Never pass
			Equal,		// ==
			LEqual,		// <=
			Greater,	// >
			NotEqual,	// !=
			GEqual,		// >=
			Always,		// Always pass
		};
		static ComparisonFunc GetComparisonByName(char const*);

		ComparisonFunc GetDepthComparison() const;
		void SetDepthComparison(ComparisonFunc);

		bool GetStencilEnabled() const;
		void SetStencilEnabled(bool);

		uint8_t GetStencilReadMask() const;
		void SetStencilReadMask(uint8_t);

		uint8_t GetStencilWriteMask() const;
		void SetStencilWriteMask(uint8_t);

		static constexpr uint8_t k_defaultStencilReadMask = std::numeric_limits<uint8_t>::max();
		static constexpr uint8_t k_defaultStencilWriteMask = std::numeric_limits<uint8_t>::max();

		enum class StencilOp
		{
			Keep,				// Keep the existing stencil data
			Zero,				// Set the stencil data to 0
			Replace,			// Set the stencil data to the reference value
			IncrementSaturate,	// Increment the stencil value by 1, and clamp the result
			DecrementSaturate,	// Decrement the stencil value by 1, and clamp the result
			Invert,				// Invert the stencil data
			Increment,			// Increment the stencil value by 1, and wrap the result if necessary
			Decrement,			// Decrement the stencil value by 1, and wrap the result if necessary
		};
		static StencilOp GetStencilOpByName(char const*);

		struct StencilOpDesc
		{
			// Note: Defaults as per D3D12:
			// https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_depth_stencil_desc#remarks
			StencilOp m_failOp = StencilOp::Keep;
			StencilOp m_depthFailOp = StencilOp::Keep;
			StencilOp m_passOp = StencilOp::Keep;
			ComparisonFunc m_comparison = ComparisonFunc::Always;
		};
		StencilOpDesc const& GetFrontFaceStencilOpDesc() const;
		void SetFrontFaceStencilOpDesc(StencilOpDesc const&);

		StencilOpDesc const& GetBackFaceStencilOpDesc() const;
		void SetBackFaceStencilOpDesc(StencilOpDesc const&);


	private:
		bool m_isDirty;

		PrimitiveTopologyType m_primitiveTopologyType;

		// Rasterizer state:
		FillMode m_fillMode;
		FaceCullingMode m_faceCullingMode;
		WindingOrder m_windingOrder;
		int m_depthBias;
		float m_depthBiasClamp;
		float m_slopeScaledDepthBias;
		bool m_depthClipEnable;
		bool m_multisampleEnable;
		bool m_antialiasedLineEnable;
		uint8_t m_forcedSampleCount; // Valid values = 0, 1, 4, 8, and optionally 16. 0 == sample count is not forced
		bool m_conservativeRaster;
		
		// Depth stencil state:
		bool m_depthTestEnable; // Is depth testing enabled?
		DepthWriteMask m_depthWriteMask;
		ComparisonFunc m_depthFunc;
		bool m_stencilEnabled;
		uint8_t m_stencilReadMask;
		uint8_t m_stencilWriteMask;
		StencilOpDesc m_frontFace;
		StencilOpDesc m_backFace;
	};


	inline int PipelineState::GetDepthBias() const
	{
		return m_depthBias;
	}


	inline void PipelineState::SetDepthBias(int depthBias)
	{
		m_depthBias = depthBias;
	}


	inline float PipelineState::GetDepthBiasClamp() const
	{
		return m_depthBiasClamp;
	}


	inline void PipelineState::SetDepthBiasClamp(float depthBiasClamp)
	{
		m_depthBiasClamp = depthBiasClamp;
	}


	inline float PipelineState::GetSlopeScaledDepthBias() const
	{
		return m_slopeScaledDepthBias;
	}


	inline void PipelineState::SetSlopeScaledDepthBias(float slopeScaledDepthBias)
	{
		m_slopeScaledDepthBias = slopeScaledDepthBias;
	}


	inline bool PipelineState::GetDepthClipEnabled() const
	{
		return m_depthClipEnable;
	}


	inline void PipelineState::SetDepthClipEnabled(bool depthClipEnable)
	{
		m_depthClipEnable = depthClipEnable;
	}


	inline bool PipelineState::GetMultiSampleEnabled() const
	{
		return m_multisampleEnable;
	}


	inline void PipelineState::SetMultiSampleEnabled(bool multisampleEnable)
	{
		m_multisampleEnable = multisampleEnable;
	}


	inline bool PipelineState::GetAntiAliasedLineEnabled() const
	{
		return m_antialiasedLineEnable;
	}


	inline void PipelineState::SetAntiAliasedLineEnabled(bool antialiasedLineEnable)
	{
		m_antialiasedLineEnable = antialiasedLineEnable;
	}


	inline uint8_t PipelineState::GetForcedSampleCount() const
	{
		return m_forcedSampleCount;
	}


	inline void PipelineState::SetForcedSampleCount(uint8_t forcedSampleCount)
	{
		SEAssert(forcedSampleCount == 0 ||
			forcedSampleCount == 1 || 
			forcedSampleCount == 4 || 
			forcedSampleCount == 8 || 
			forcedSampleCount == 16,
			"Invalid forced sample count");
		m_forcedSampleCount = forcedSampleCount;
	}


	inline bool PipelineState::GetConservativeRaster() const
	{
		return m_conservativeRaster;
	}


	inline void PipelineState::SetConservativeRaster(bool conservativeRaster)
	{
		m_conservativeRaster = conservativeRaster;
	}


	inline bool PipelineState::GetDepthTestEnabled() const
	{
		return m_depthTestEnable;
	}


	inline void PipelineState::SetDepthTestEnabled(bool depthTestEnable)
	{
		m_depthTestEnable = depthTestEnable;
	}


	inline PipelineState::DepthWriteMask PipelineState::GetDepthWriteMask() const
	{
		return m_depthWriteMask;
	}


	inline void PipelineState::SetDepthWriteMask(DepthWriteMask depthWriteMask)
	{
		m_depthWriteMask = depthWriteMask;
	}


	inline bool PipelineState::GetStencilEnabled() const
	{
		return m_stencilEnabled;
	}


	inline void PipelineState::SetStencilEnabled(bool stencilEnabled)
	{
		m_stencilEnabled = stencilEnabled;
	}


	inline uint8_t PipelineState::GetStencilReadMask() const
	{
		return m_stencilReadMask;
	}


	inline void PipelineState::SetStencilReadMask(uint8_t stencilReadMask)
	{
		m_stencilReadMask = stencilReadMask;
	}


	inline uint8_t PipelineState::GetStencilWriteMask() const
	{
		return m_stencilWriteMask;
	}


	inline void PipelineState::SetStencilWriteMask(uint8_t stencilWriteMask)
	{
		m_stencilWriteMask = stencilWriteMask;
	}


	inline PipelineState::StencilOpDesc const& PipelineState::GetFrontFaceStencilOpDesc() const
	{
		return m_frontFace;
	}


	inline void PipelineState::SetFrontFaceStencilOpDesc(StencilOpDesc const& frontFace)
	{
		m_frontFace = frontFace;
	}


	inline PipelineState::StencilOpDesc const& PipelineState::GetBackFaceStencilOpDesc() const
	{
		return m_backFace;
	}


	inline void PipelineState::SetBackFaceStencilOpDesc(StencilOpDesc const& backFace)
	{
		m_backFace = backFace;
	}
}