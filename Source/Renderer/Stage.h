// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Batch.h"
#include "BatchHandle.h"
#include "BufferView.h"
#include "Effect.h"
#include "MeshFactory.h"
#include "RootConstants.h"
#include "SysInfo_Platform.h"
#include "TextureView.h"
#include "TextureTarget.h"

#include "Core/InvPtr.h"

#include "Core/Interfaces/INamedObject.h"


namespace effect
{
	class EffectDB;
}
namespace gr
{
	class IndexedBufferManager;
}

namespace re
{
	class Buffer;
	class BufferInput;
	class Shader;
	class Texture;
}

namespace gr
{
	class ComputeStage;
	class ClearTargetSetStage;
	class ClearRWTexturesStage;
	class CopyStage;

	class Stage : public virtual core::INamedObject
	{
	public:
		static constexpr int k_noDepthTexAsInputFlag = -1;

		enum class Type
		{
			Parent, // Does not contribute batches

			// Raster queue:
			Raster,
			FullscreenQuad,
			ClearTargetSet,
			LibraryRaster,
			Copy,

			// Compute queue:
			Compute,
			LibraryCompute,
			ClearRWTextures,

			RayTracing,

			Invalid
		};
		static constexpr bool IsLibraryType(Type type);

		struct IStageParams
		{
			virtual ~IStageParams() = default;
		};
		struct GraphicsStageParams final : public virtual IStageParams
		{
			// TODO: Populate this
			// Assert values are set when they're received to catch any GS's that need to be updated
		};
		struct ComputeStageParams final : public virtual IStageParams
		{
			// TODO: Populate this
		};
		struct LibraryStageParams final : public virtual IStageParams
		{
			Type m_stageType;

			enum class LibraryType
			{
				ImGui,
			} m_type;

			LibraryStageParams(Type stageType, LibraryType type) : m_stageType(stageType), m_type(type) {}

			std::shared_ptr<void> m_payload; // Interpreted by the library wrapper

		private:
			LibraryStageParams() = delete;
		};
		struct FullscreenQuadParams final : public virtual IStageParams
		{
			gr::meshfactory::ZLocation m_zLocation = gr::meshfactory::ZLocation::Near;

			EffectID m_effectID;
			effect::drawstyle::Bitmask m_drawStyleBitmask = effect::drawstyle::DefaultTechnique;
		};
		struct RayTracingStageParams final : public virtual IStageParams
		{
			//
		};


	public:
		static std::shared_ptr<Stage> CreateParentStage(char const* name);

		static std::shared_ptr<Stage> CreateGraphicsStage(char const* name, GraphicsStageParams const&);
		static std::shared_ptr<Stage> CreateSingleFrameGraphicsStage(char const* name, GraphicsStageParams const&);

		static std::shared_ptr<Stage> CreateComputeStage(char const* name, ComputeStageParams const&);
		static std::shared_ptr<Stage> CreateSingleFrameComputeStage(char const* name, ComputeStageParams const&);

		static std::shared_ptr<Stage> CreateLibraryStage(char const* name, LibraryStageParams const&);

		static std::shared_ptr<Stage> CreateFullscreenQuadStage(char const* name, FullscreenQuadParams const&);
		static std::shared_ptr<Stage> CreateSingleFrameFullscreenQuadStage(char const* name, FullscreenQuadParams const&);

		static std::shared_ptr<Stage> CreateRayTracingStage(char const* name, RayTracingStageParams const&);
		static std::shared_ptr<Stage> CreateSingleFrameRayTracingStage(char const* name, RayTracingStageParams const&);

		static std::shared_ptr<ClearTargetSetStage> CreateTargetSetClearStage(
			char const* name, std::shared_ptr<re::TextureTargetSet> const&);
		static std::shared_ptr<ClearTargetSetStage> CreateSingleFrameTargetSetClearStage(
			char const* name, std::shared_ptr<re::TextureTargetSet>const&);

		static std::shared_ptr<ClearRWTexturesStage> CreateRWTextureClearStage(char const* name);
		static std::shared_ptr<ClearRWTexturesStage> CreateSingleFrameRWTextureClearStage(char const* name);

		static std::shared_ptr<CopyStage> CreateCopyStage(
			core::InvPtr<re::Texture> const& src, core::InvPtr<re::Texture> const& dst); // Uses backbuffer if dst is null/invalid
		static std::shared_ptr<CopyStage> CreateSingleFrameCopyStage(
			core::InvPtr<re::Texture> const& src, core::InvPtr<re::Texture> const& dst); // Uses backbuffer if dst is null/invalid


	public:
		virtual ~Stage() = default;

		Stage(Stage&&) noexcept = default;
		Stage& operator=(Stage&&) noexcept = default;

		void PostUpdatePreRender(gr::IndexedBufferManager&, effect::EffectDB const&);
		void EndOfFrame(); // Clears per-frame data. Called by the owning RenderPipeline

		bool IsSkippable() const;

		Type GetStageType() const;
		re::Lifetime GetStageLifetime() const;
		IStageParams const* GetStageParams() const;

		void AddDrawStyleBits(effect::drawstyle::Bitmask);
		void ClearDrawStyleBits();

		re::TextureTargetSet const* GetTextureTargetSet() const;
		re::TextureTargetSet* GetTextureTargetSet();
		void SetTextureTargetSet(std::shared_ptr<re::TextureTargetSet> const& targetSet);

		void AddPermanentTextureInput(
			char const* shaderName,
			core::InvPtr<re::Texture> const&,
			core::InvPtr<re::Sampler> const&,
			re::TextureView const&);

		void AddPermanentTextureInput(
			std::string const& shaderName,
			core::InvPtr<re::Texture> const&,
			core::InvPtr<re::Sampler> const&,
			re::TextureView const&);

		std::vector<re::TextureAndSamplerInput> const& GetPermanentTextureInputs() const;

		void AddSingleFrameTextureInput(
			char const* shaderName,
			core::InvPtr<re::Texture> const&,
			core::InvPtr<re::Sampler> const&,
			re::TextureView const&);

		void AddSingleFrameTextureInput(
			std::string const& shaderName,
			core::InvPtr<re::Texture> const&,
			core::InvPtr<re::Sampler> const&,
			re::TextureView const&);

		std::vector<re::TextureAndSamplerInput> const& GetSingleFrameTextureInputs() const;

		void AddPermanentRWTextureInput(
			std::string const& shaderName,
			core::InvPtr<re::Texture> const&,
			re::TextureView const&);

		std::vector<re::RWTextureInput> const& GetPermanentRWTextureInputs() const;

		void AddSingleFrameRWTextureInput(
			char const* shaderName,
			core::InvPtr<re::Texture> const&,
			re::TextureView const&);

		std::vector<re::RWTextureInput> const& GetSingleFrameRWTextureInputs() const;

		bool DepthTargetIsAlsoTextureInput() const;
		int GetDepthTargetTextureInputIdx() const;

		void AddPermanentBuffer(std::string const& shaderName, std::shared_ptr<re::Buffer> const&); // Infer a default BufferView
		void AddPermanentBuffer(std::string const& shaderName, std::shared_ptr<re::Buffer> const&, re::BufferView const&);
		void AddPermanentBuffer(re::BufferInput&&);
		void AddPermanentBuffer(re::BufferInput const&);
		inline std::vector<re::BufferInput> const& GetPermanentBuffers() const;

		void AddSingleFrameBuffer(std::string const& shaderName, std::shared_ptr<re::Buffer> const&); // Infer a default BufferView
		void AddSingleFrameBuffer(std::string const& shaderName, std::shared_ptr<re::Buffer> const&, re::BufferView const&);
		void AddSingleFrameBuffer(re::BufferInput&&);
		void AddSingleFrameBuffer(re::BufferInput const&);
		inline std::vector<re::BufferInput> const& GetPerFrameBuffers() const;

		void SetRootConstant(char const* shaderName, void const* src, re::DataType);
		void SetRootConstant(std::string const& shaderName, void const* src, re::DataType);
		re::RootConstants const& GetRootConstants() const;

		// Stage Batches:
		std::vector<gr::StageBatchHandle> const& GetStageBatches() const;
		
		void AddBatches(std::vector<gr::BatchHandle> const&);
		
		gr::StageBatchHandle* AddBatch(gr::BatchHandle); // Returns Batch ptr (IFF it was successfully added)

		void SetInstancingEnabled(bool instancingEnabled); // Raster stages only

		enum class FilterMode
		{
			Require,
			Exclude
		};
		void SetBatchFilterMaskBit(re::Batch::Filter filterBit, FilterMode, bool enabled);


	protected:
		explicit Stage(char const* name, std::unique_ptr<IStageParams>&&, Type, re::Lifetime);


	private:
		void UpdateDepthTextureInputIndex();
		void ResolveBatches(gr::IndexedBufferManager&, effect::EffectDB const&);

		void ValidateTexturesAndTargets();


	protected:
		const Type m_type;
		const re::Lifetime m_lifetime;
		std::unique_ptr<IStageParams> m_stageParams;

		std::shared_ptr<re::TextureTargetSet> m_textureTargetSet;
		std::vector<re::TextureAndSamplerInput> m_permanentTextureSamplerInputs;
		std::vector<re::TextureAndSamplerInput> m_singleFrameTextureSamplerInputs;
		int m_depthTextureInputIdx; // k_noDepthTexAsInputFlag: Depth not attached as an input		

		std::vector<re::RWTextureInput> m_permanentRWTextureInputs;
		std::vector<re::RWTextureInput> m_singleFrameRWTextureInputs;

		std::vector<re::BufferInput> m_singleFrameBuffers; // Cleared every frame

		std::vector<re::BufferInput> m_permanentBuffers;

		re::RootConstants m_stageRootConstants;

		std::vector<gr::StageBatchHandle> m_resolvedBatches;

		re::Batch::FilterBitmask m_requiredBatchFilterBitmasks;
		re::Batch::FilterBitmask m_excludedBatchFilterBitmasks;

		effect::drawstyle::Bitmask m_drawStyleBits;

		bool m_instancingEnabled; // Raster stages only

	private:
		Stage() = delete;
		Stage(Stage const&) = delete;
		Stage& operator=(Stage const&) = delete;
	};


	inline void Stage::SetInstancingEnabled(bool instancingEnabled)
	{
		SEAssert(m_type == gr::Stage::Type::Raster || m_type == gr::Stage::Type::LibraryRaster,
			"Invalid stage type for instancing");
		m_instancingEnabled = instancingEnabled;
	}


	//---


	class ParentStage final : public virtual Stage
	{
	public:
		// 

	private:
		ParentStage(char const* name, re::Lifetime);
		friend class Stage;
	};


	//---


	class ComputeStage final : public virtual Stage
	{
	public:
		// 

	private:
		ComputeStage(char const* name, std::unique_ptr<ComputeStageParams>&&, re::Lifetime);
		friend class Stage;
	};


	//---


	class FullscreenQuadStage final : public virtual Stage
	{
	public:
		//

	private:
		core::InvPtr<gr::MeshPrimitive> m_screenAlignedQuad;
		gr::BatchHandle m_fullscreenQuadBatch;

	private:
		FullscreenQuadStage(char const* name, std::unique_ptr<FullscreenQuadParams>&&, re::Lifetime);
		friend class Stage;
	};


	//---


	class RayTracingStage final : public virtual Stage
	{
	public:
		// 

	private:
		RayTracingStage(char const* name, std::unique_ptr<RayTracingStageParams>&&, re::Lifetime);
		friend class Stage;
	};


	//---


	class ClearTargetSetStage final : public virtual Stage
	{
	public:
		void EnableAllColorClear(glm::vec4 const& colorClearVal = glm::vec4(0.f));
		void EnableColorClear(uint8_t idx, glm::vec4 const& colorClearVal = glm::vec4(0.f));

		void EnableDepthClear(float clearVal = 1.f);

		void EnableStencilClear(uint8_t clearVal = 0);


	public:
		bool ColorClearEnabled() const; // Is color clearing enabled for any target?
		bool ColorClearEnabled(uint8_t idx) const; // Is color clearing enabled for a specific target index?

		bool const* GetAllColorClearModes() const;
		glm::vec4 const* GetAllColorClearValues() const;
		uint8_t GetNumColorClearElements() const;

		bool DepthClearEnabled() const;
		float GetDepthClearValue() const;

		bool StencilClearEnabled() const;
		uint8_t GetStencilClearValue() const;


	private:
		std::unique_ptr<bool[]> m_colorClearModes;
		std::unique_ptr<glm::vec4[]> m_colorClearValues;

		float m_depthClearVal;
		uint8_t m_stencilClearVal;

		uint8_t m_numColorClears;
		bool m_depthClearMode;
		bool m_stencilClearMode;


	private:
		ClearTargetSetStage(char const* name, re::Lifetime);
		friend class Stage;
	};


	inline void ClearTargetSetStage::EnableAllColorClear(glm::vec4 const& colorClearVal /*= glm::vec4(0.f)*/)
	{
		SEAssert(m_colorClearModes == nullptr && m_colorClearValues == nullptr,
			"Clear mode already set. This function should only be called once");

		m_numColorClears = platform::SysInfo::GetMaxRenderTargets();

		m_colorClearModes = std::unique_ptr<bool[]>(new bool[m_numColorClears]);
		m_colorClearValues = std::unique_ptr<glm::vec4[]>(new glm::vec4[m_numColorClears]);

		for (uint8_t i = 0; i < m_numColorClears; ++i)
		{
			m_colorClearModes[i] = true;
			m_colorClearValues[i] = glm::vec4(0.f);
		}
	}


	inline void ClearTargetSetStage::EnableColorClear(uint8_t idx, glm::vec4 const& colorClearVal /*= glm::vec4(0.f)*/)
	{
		SEAssert((m_colorClearModes == nullptr) == (m_colorClearValues == nullptr),
			"Color clear members are out of sync");

		if (m_colorClearModes == nullptr)
		{
			m_numColorClears = platform::SysInfo::GetMaxRenderTargets();

			m_colorClearModes = std::unique_ptr<bool[]>(new bool[m_numColorClears]);
			m_colorClearValues = std::unique_ptr<glm::vec4[]>(new glm::vec4[m_numColorClears]);

			for (uint8_t i = 0; i < m_numColorClears; ++i)
			{
				m_colorClearModes[i] = false;
				m_colorClearValues[i] = glm::vec4(0.f);
			}
		}

		SEAssert(idx < platform::SysInfo::GetMaxRenderTargets(), "OOB index");

		m_colorClearModes[idx] = true;
		m_colorClearValues[idx] = colorClearVal;
	}


	inline void ClearTargetSetStage::EnableDepthClear(float clearVal /*= 1.f*/)
	{
		m_depthClearVal = clearVal;
		m_depthClearMode = true;
	}


	inline void ClearTargetSetStage::EnableStencilClear(uint8_t clearVal /*= 0*/)
	{
		m_stencilClearVal = clearVal;
		m_stencilClearMode = true;
	}


	inline bool ClearTargetSetStage::ColorClearEnabled() const
	{
		SEAssert((m_colorClearModes == nullptr) == (m_colorClearValues == nullptr),
			"Color clear members are out of sync");

		return m_colorClearModes != nullptr;
	}


	inline bool ClearTargetSetStage::ColorClearEnabled(uint8_t idx) const
	{
		SEAssert((m_colorClearModes == nullptr) == (m_colorClearValues == nullptr),
			"Color clear members are out of sync");

		if (ColorClearEnabled())
		{
			SEAssert(idx < m_numColorClears, "OOB index");

			return m_colorClearModes[idx];
		}
		return false;
	}


	inline bool const* ClearTargetSetStage::GetAllColorClearModes() const
	{
		return m_colorClearModes.get();
	}


	inline glm::vec4 const* ClearTargetSetStage::GetAllColorClearValues() const
	{
		return m_colorClearValues.get();
	}


	inline uint8_t ClearTargetSetStage::GetNumColorClearElements() const
	{
		return m_numColorClears;
	}


	inline bool ClearTargetSetStage::DepthClearEnabled() const
	{
		return m_depthClearMode;
	}


	inline float ClearTargetSetStage::GetDepthClearValue() const
	{
		return m_depthClearVal;
	}


	inline bool ClearTargetSetStage::StencilClearEnabled() const
	{
		return m_stencilClearMode;
	}


	inline uint8_t ClearTargetSetStage::GetStencilClearValue() const
	{
		return m_stencilClearVal;
	}


	//---


	class ClearRWTexturesStage final : public virtual Stage
	{
	public:
		enum class ValueType : bool
		{
			Float,
			Uint,
		} m_clearValueType;


	public: // Set the clear value for ALL RW textures set on the stage
		void SetClearValue(glm::vec4 const&);
		void SetClearValue(glm::uvec4 const&);

		ValueType GetClearValueType() const;
		void const* GetClearValue() const;


	public: // Override the Stage version, as we don't need shader names
		void AddPermanentRWTextureInput(core::InvPtr<re::Texture> const&, re::TextureView const&);
		void AddSingleFrameRWTextureInput(core::InvPtr<re::Texture> const&, re::TextureView const&);


	private:
		union
		{
			glm::vec4 m_clearFloat;
			glm::uvec4 m_clearUint;
		};

		static constexpr char const* k_dummyShaderName = "UnusedTextureShaderName"; // Used to populate RWTextureInputs


	private:
		ClearRWTexturesStage(char const* name, re::Lifetime);
		friend class Stage;
	};


	inline void ClearRWTexturesStage::SetClearValue(glm::vec4 const& floatVal)
	{
		m_clearFloat = floatVal;
		m_clearValueType = ValueType::Float;
	}


	inline void ClearRWTexturesStage::SetClearValue(glm::uvec4 const& uintVal)
	{
		m_clearUint = uintVal;
		m_clearValueType = ValueType::Uint;
	}


	inline ClearRWTexturesStage::ValueType ClearRWTexturesStage::GetClearValueType() const
	{
		return m_clearValueType;
	}


	inline void const* ClearRWTexturesStage::GetClearValue() const
	{
		switch (m_clearValueType)
		{
		case ValueType::Float: return &m_clearFloat.x;
		case ValueType::Uint: return &m_clearUint.x;
		default: SEAssertF("Invalid clear value type");
		}
		return nullptr; // This should never happen
	}


	//---


	class CopyStage final : public virtual Stage
	{
	public:
		core::InvPtr<re::Texture> const& GetSrcTexture() const;
		core::InvPtr<re::Texture> const& GetDstTexture() const;

	private:
		core::InvPtr<re::Texture> m_src;
		core::InvPtr<re::Texture> m_dst;


	private:
		friend class Stage;
		CopyStage(char const* name, 
			re::Lifetime, 
			core::InvPtr<re::Texture> const& src, 
			core::InvPtr<re::Texture> const& dst); // If dst is not valid, copies to backbuffer
	};


	inline core::InvPtr<re::Texture> const& CopyStage::GetSrcTexture() const
	{
		return m_src;
	}


	inline core::InvPtr<re::Texture> const& CopyStage::GetDstTexture() const
	{
		return m_dst;
	}


	//---


	class LibraryStage final : public virtual Stage
	{
	public:
		struct IPayload
		{
			virtual ~IPayload() = default;
		};

	public:
		void Execute(void* platformObject); // e.g. platformObject == DX12 command list
		
		// The payload is an arbitrary data blob passed by a graphics system every frame for consumption by the backend
		void SetPayload(std::unique_ptr<IPayload>&&);
		std::unique_ptr<IPayload> TakePayload();

	private:
		std::unique_ptr<IPayload> m_payload;

	private:
		LibraryStage(char const* name, std::unique_ptr<LibraryStageParams>&&, re::Lifetime);
		friend class Stage;
	};


	//---


	inline constexpr bool Stage::IsLibraryType(Type type)
	{
		return type == Type::LibraryRaster || type == Type::LibraryCompute;
	}


	inline Stage::Type Stage::GetStageType() const
	{
		return m_type;
	}


	inline re::Lifetime Stage::GetStageLifetime() const
	{
		return m_lifetime;
	}


	inline Stage::IStageParams const* Stage::GetStageParams() const
	{
		return m_stageParams.get();
	}


	inline void Stage::AddDrawStyleBits(effect::drawstyle::Bitmask drawStyleBits)
	{
		SEAssert(m_type == gr::Stage::Type::Raster || m_type == gr::Stage::Type::Compute,
			"Unexpected stage type for setting a drawstyle on");
		m_drawStyleBits |= drawStyleBits;
	}


	inline void Stage::ClearDrawStyleBits()
	{
		m_drawStyleBits = 0;
	}


	inline re::TextureTargetSet const* Stage::GetTextureTargetSet() const
	{
		return m_textureTargetSet.get();
	}


	inline re::TextureTargetSet* Stage::GetTextureTargetSet()
	{
		return m_textureTargetSet.get();
	}


	inline std::vector<re::TextureAndSamplerInput> const& Stage::GetPermanentTextureInputs() const
	{
		return m_permanentTextureSamplerInputs;
	}


	inline std::vector<re::TextureAndSamplerInput> const& Stage::GetSingleFrameTextureInputs() const
	{
		return m_singleFrameTextureSamplerInputs;
	}


	inline std::vector<re::RWTextureInput> const& Stage::GetPermanentRWTextureInputs() const
	{
		return m_permanentRWTextureInputs;
	}


	inline std::vector<re::RWTextureInput> const& Stage::GetSingleFrameRWTextureInputs() const
	{
		return m_singleFrameRWTextureInputs;
	}


	inline bool Stage::DepthTargetIsAlsoTextureInput() const
	{
		return m_depthTextureInputIdx != k_noDepthTexAsInputFlag;
	}


	inline int Stage::GetDepthTargetTextureInputIdx() const
	{
		return m_depthTextureInputIdx;
	}


	inline std::vector<re::BufferInput> const& Stage::GetPermanentBuffers() const
	{
		return m_permanentBuffers;
	}


	inline std::vector<re::BufferInput> const& Stage::GetPerFrameBuffers() const
	{
		return m_singleFrameBuffers;
	}


	inline void Stage::SetRootConstant(char const* shaderName, void const* src, re::DataType dataType)
	{
		SEAssert(m_type != Stage::Type::ClearTargetSet &&
			m_type != Stage::Type::Copy &&
			m_type != Stage::Type::ClearRWTextures,
			"Invalid stage type for setting root parameters");

		m_stageRootConstants.SetRootConstant(shaderName, src, dataType);
	}


	inline void Stage::SetRootConstant(std::string const& shaderName, void const* src, re::DataType dataType)
	{
		m_stageRootConstants.SetRootConstant(shaderName, src, dataType);
	}

	inline re::RootConstants const& Stage::GetRootConstants() const
	{
		return m_stageRootConstants;
	}


	inline std::vector<gr::StageBatchHandle> const& Stage::GetStageBatches() const
	{
		return m_resolvedBatches;
	}
}