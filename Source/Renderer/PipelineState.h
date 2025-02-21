// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Core/Interfaces/IHashedDataObject.h"


namespace re
{
	class PipelineState : public virtual core::IHashedDataObject
	{
	public:
		PipelineState();
		
		PipelineState(PipelineState const&) = default;
		PipelineState(PipelineState&&) noexcept = default;
		PipelineState& operator=(PipelineState const&) = default;
		PipelineState& operator=(PipelineState&&) noexcept = default;
		~PipelineState() = default;
			
	public: // IHashedDataObject:
		util::HashKey GetDataHash() const override;

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


		// Blend state:
		//-------------
		enum class BlendMode : uint8_t // Graphics stages only
		{
			// Note: See https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_blend#constants
			Zero,
			One,
			SrcColor,
			InvSrcColor,
			SrcAlpha,
			InvSrcAlpha,
			DstAlpha,
			InvDstAlpha,
			DstColor,
			InvDstColor,
			SrcAlphaSat,
			BlendFactor,
			InvBlendFactor,
			SrcOneColor,
			InvSrcOneColor,
			SrcOneAlpha,
			InvSrcOneAlpha,
			AlphaFactor,
			InvAlphaFactor,
		};
		static BlendMode GetBlendModeByName(char const*);

		enum class BlendOp
		{
			// Note: See https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_blend_op#constants
			Add,
			Subtract,
			RevSubtract,
			Min,
			Max,
		};
		static BlendOp GetBlendOpByName(char const*);

		enum class LogicOp
		{
			// Note: See https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_logic_op#constants
			Clear,
			Set,
			Copy,
			CopyInverted,
			NoOp,
			Invert,
			AND,
			NAND,
			OR,
			NOR,
			XOR,
			EQUIV,
			ANDReverse,
			AndInverted,
			ORReverse,
			ORInverted,
		};
		static LogicOp GetLogicOpByName(char const*);

		enum ColorWriteEnable : uint8_t
		{
			Red		= 1 << 0,
			Green	= 1 << 1,
			Blue	= 1 << 2,
			Alpha	= 1 << 3,
			All		= (Red | Green | Blue | Alpha),
		};

		struct RenderTargetBlendDesc
		{
			bool m_blendEnable = false;
			bool m_logicOpEnable = false;
			BlendMode m_srcBlend = BlendMode::One;
			BlendMode m_dstBlend = BlendMode::Zero;
			BlendOp m_blendOp = BlendOp::Add;
			BlendMode m_srcBlendAlpha = BlendMode::One;
			BlendMode m_dstBlendAlpha = BlendMode::Zero;
			BlendOp m_blendOpAlpha = BlendOp::Add;
			LogicOp m_logicOp = LogicOp::NoOp;
			uint8_t m_renderTargetWriteMask = ColorWriteEnable::All;
		};

		bool GetAlphaToCoverageEnabled() const;
		void SetAlphaToCoverageEnabled(bool);

		bool GetIndependentBlendEnabled() const;
		void SetIndependentBlendEnabled(bool);

		std::array<RenderTargetBlendDesc, 8> const& GetRenderTargetBlendDescs() const;
		void SetRenderTargetBlendDesc(RenderTargetBlendDesc const&, uint8_t index);


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

		// Blend state:
		bool m_alphaToCoverageEnable;
		bool m_independentBlendEnable;
		std::array<RenderTargetBlendDesc, 8> m_renderTargetBlendDescs;
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


	inline bool PipelineState::GetAlphaToCoverageEnabled() const
	{
		return m_alphaToCoverageEnable;
	}


	inline void PipelineState::SetAlphaToCoverageEnabled(bool alphaToCoverageEnable)
	{
		m_alphaToCoverageEnable = alphaToCoverageEnable;
	}


	inline bool PipelineState::GetIndependentBlendEnabled() const
	{
		return m_independentBlendEnable;
	}


	inline void PipelineState::SetIndependentBlendEnabled(bool independentBlendEnable)
	{
		m_independentBlendEnable = independentBlendEnable;
	}


	inline std::array<PipelineState::RenderTargetBlendDesc, 8> const& PipelineState::GetRenderTargetBlendDescs() const
	{
		return m_renderTargetBlendDescs;
	}


	inline void PipelineState::SetRenderTargetBlendDesc(RenderTargetBlendDesc const& blendDesc, uint8_t index)
	{
		SEAssert(blendDesc.m_logicOpEnable != blendDesc.m_blendEnable ||
			(!blendDesc.m_logicOpEnable &&
				!blendDesc.m_blendEnable),
			"It is not valid for logic op and blend to both be enabled");

		m_renderTargetBlendDescs[index] = blendDesc;
	}
}