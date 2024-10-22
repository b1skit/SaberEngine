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


		//
		//----
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
		
		//
		DepthTestMode m_depthTestMode;
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
}