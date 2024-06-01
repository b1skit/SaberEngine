// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Batch.h"
#include "Effect.h"
#include "MeshFactory.h"
#include "TextureTarget.h"

#include "Core\Interfaces\INamedObject.h"


namespace re
{
	class ComputeStage;
	class Buffer;
	class Shader;
	class Texture;


	class RenderStage : public virtual core::INamedObject
	{
	public:
		static constexpr int k_noDepthTexAsInputFlag = -1;

		enum class Lifetime
		{
			SingleFrame,
			Permanent
		};
		enum class Type
		{
			Parent, // Does not contribute batches
			Graphics,
			Compute,
			
			Library, // Wrapper for external libraries

			FullscreenQuad, // Graphics queue
			Clear, // Graphics queue

			Invalid
		};
		struct IStageParams
		{
			virtual ~IStageParams() = 0;
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
			enum class LibraryType
			{
				ImGui,
			} m_type;

			LibraryStageParams(LibraryType type) : m_type(type) {}

			std::shared_ptr<void> m_payload; // Interpreted by the library wrapper

		private:
			LibraryStageParams() = delete;
		};
		struct FullscreenQuadParams final : public virtual IStageParams
		{
			gr::meshfactory::ZLocation m_zLocation = gr::meshfactory::ZLocation::Near;

			EffectID m_effectID = effect::Effect::k_invalidEffectID;
			effect::DrawStyle::Bitmask m_drawStyleBitmask = effect::DrawStyle::k_defaultTechniqueBitmask;
		};
		struct ClearStageParams final : public virtual IStageParams
		{
			// 1 entry: applied to all targets, or per-target if m_colorClearMode.size() == targetSet.GetNumColorTargets()
			std::vector<re::TextureTarget::TargetParams::ClearMode> m_colorClearModes;
			glm::vec4 m_clearColor = glm::vec4(0.f, 0.f, 0.f, 0.f);

			re::TextureTarget::TargetParams::ClearMode m_depthClearMode = 
				re::TextureTarget::TargetParams::ClearMode::Disabled;
			float m_clearDepth = 1.f; // Far plane
		};

		struct RenderStageTextureAndSamplerInput
		{
			std::string m_shaderName;
			re::Texture const* m_texture;
			std::shared_ptr<Sampler> m_sampler;

			uint32_t m_srcMip = re::Texture::k_allMips;
		};


	public:
		static std::shared_ptr<RenderStage> CreateParentStage(char const* name);

		static std::shared_ptr<RenderStage> CreateGraphicsStage(char const* name, GraphicsStageParams const&);
		static std::shared_ptr<RenderStage> CreateSingleFrameGraphicsStage(char const* name, GraphicsStageParams const&);

		static std::shared_ptr<RenderStage> CreateComputeStage(char const* name, ComputeStageParams const&);
		static std::shared_ptr<RenderStage> CreateSingleFrameComputeStage(char const* name, ComputeStageParams const&);

		static std::shared_ptr<RenderStage> CreateLibraryStage(char const* name, LibraryStageParams const&);

		static std::shared_ptr<RenderStage> CreateFullscreenQuadStage(char const* name, FullscreenQuadParams const&);
		static std::shared_ptr<RenderStage> CreateSingleFrameFullscreenQuadStage(char const* name, FullscreenQuadParams const&);

		static std::shared_ptr<RenderStage> CreateClearStage(
			ClearStageParams const&, std::shared_ptr<re::TextureTargetSet const>);
		static std::shared_ptr<RenderStage> CreateSingleFrameClearStage(
			ClearStageParams const&, std::shared_ptr<re::TextureTargetSet const>);


	public:
		~RenderStage() = default;

		void EndOfFrame(); // Clears per-frame data. Called by the owning RenderPipeline

		bool IsSkippable() const;

		Type GetStageType() const;
		Lifetime GetStageLifetime() const;
		IStageParams const* GetStageParams() const;

		void SetDrawStyle(effect::DrawStyle::Bitmask);
		void ClearDrawStyle();

		re::TextureTargetSet const* GetTextureTargetSet() const;
		void SetTextureTargetSet(std::shared_ptr<re::TextureTargetSet> targetSet);

		void AddTextureInput(
			std::string const& shaderName,
			re::Texture const*,
			std::shared_ptr<re::Sampler>,
			uint32_t mipLevel = re::Texture::k_allMips);
		void AddTextureInput(
			std::string const& shaderName, 
			std::shared_ptr<re::Texture>, 
			std::shared_ptr<re::Sampler>, 
			uint32_t mipLevel = re::Texture::k_allMips);
		std::vector<RenderStage::RenderStageTextureAndSamplerInput> const& GetTextureInputs() const;

		bool DepthTargetIsAlsoTextureInput() const;
		int GetDepthTargetTextureInputIdx() const;

		void AddPermanentBuffer(std::shared_ptr<re::Buffer const>);
		inline std::vector<std::shared_ptr<re::Buffer const>> const& GetPermanentBuffers() const;
		
		void AddSingleFrameBuffer(std::shared_ptr<re::Buffer const>);
		inline std::vector<std::shared_ptr<re::Buffer const>> const& GetPerFrameBuffers() const;

		// Stage Batches:
		std::vector<re::Batch> const& GetStageBatches() const;
		void AddBatches(std::vector<re::Batch> const&);
		void AddBatch(re::Batch const&);

		enum class FilterMode
		{
			Require,
			Exclude
		};
		void SetBatchFilterMaskBit(re::Batch::Filter filterBit, FilterMode, bool enabled);


	protected:
		explicit RenderStage(char const* name, std::unique_ptr<IStageParams>&&, Type, Lifetime);


	private:
		void UpdateDepthTextureInputIndex();
		void ValidateTexturesAndTargets();
		

	protected:
		const Type m_type;
		const Lifetime m_lifetime;
		std::unique_ptr<IStageParams> m_stageParams;

		std::shared_ptr<re::TextureTargetSet> m_textureTargetSet;
		std::vector<RenderStageTextureAndSamplerInput> m_textureSamplerInputs;
		int m_depthTextureInputIdx; // k_noDepthTexAsInputFlag: Depth not attached as an input		

		std::vector<std::shared_ptr<re::Buffer const>> m_singleFrameBuffers; // Cleared every frame

		std::vector<std::shared_ptr<re::Buffer const>> m_permanentBuffers;

		std::vector<re::Batch> m_stageBatches;

		re::Batch::FilterBitmask m_requiredBatchFilterBitmasks;
		re::Batch::FilterBitmask m_excludedBatchFilterBitmasks;

		effect::DrawStyle::Bitmask m_drawStyleBits;

		
	private:
		RenderStage() = delete;
		RenderStage(RenderStage const&) = delete;
		RenderStage(RenderStage&&) = delete;
		RenderStage& operator=(RenderStage const&) = delete;
	};


	//---


	class ParentStage final : public virtual RenderStage
	{
	public:
		// 

	private:
		ParentStage(char const* name, Lifetime);
		friend class RenderStage;
	};


	//---


	class ComputeStage final : public virtual RenderStage
	{
	public:
		// 

	private:
		ComputeStage(char const* name, std::unique_ptr<ComputeStageParams>&&, Lifetime);
		friend class RenderStage;
	};


	//---


	class FullscreenQuadStage final : public virtual RenderStage
	{
	public:
		//

	private:
		std::shared_ptr<gr::MeshPrimitive> m_screenAlignedQuad;
		std::unique_ptr<re::Batch> m_fullscreenQuadBatch;

	private:
		FullscreenQuadStage(char const* name, std::unique_ptr<FullscreenQuadParams>&&, Lifetime);
		friend class RenderStage;
	};


	//---


	class ClearStage final : public virtual RenderStage
	{
	public:
		// 

	private:
		ClearStage(char const* name, Lifetime);
		friend class RenderStage;
	};


	//---


	class LibraryStage final : public virtual RenderStage
	{
	public:
		struct IPayload
		{
			virtual ~IPayload() = 0;
		};

	public:
		void Execute();
		
		// The payload is an arbitrary data blob passed by a graphics system every frame for consumption by the backend
		void SetPayload(std::unique_ptr<IPayload>&&);
		std::unique_ptr<IPayload> TakePayload();

	private:
		std::unique_ptr<IPayload> m_payload;

	private:
		LibraryStage(char const* name, std::unique_ptr<LibraryStageParams>&&, Lifetime);
		friend class RenderStage;
	};


	//---


	inline RenderStage::Type RenderStage::GetStageType() const
	{
		return m_type;
	}


	inline RenderStage::Lifetime RenderStage::GetStageLifetime() const
	{
		return m_lifetime;
	}


	inline RenderStage::IStageParams const* RenderStage::GetStageParams() const
	{
		return m_stageParams.get();
	}


	inline void RenderStage::SetDrawStyle(effect::DrawStyle::Bitmask drawStyleBits)
	{
		m_drawStyleBits |= drawStyleBits;
	}


	inline void RenderStage::ClearDrawStyle()
	{
		m_drawStyleBits = 0;
	}


	inline re::TextureTargetSet const* RenderStage::GetTextureTargetSet() const
	{
		return m_textureTargetSet.get();
	}


	inline std::vector<RenderStage::RenderStageTextureAndSamplerInput> const& RenderStage::GetTextureInputs() const
	{
		return m_textureSamplerInputs;
	}


	inline bool RenderStage::DepthTargetIsAlsoTextureInput() const
	{
		return m_depthTextureInputIdx != k_noDepthTexAsInputFlag;
	}


	inline int RenderStage::GetDepthTargetTextureInputIdx() const
	{
		return m_depthTextureInputIdx;
	}


	inline std::vector<std::shared_ptr<re::Buffer const>> const& RenderStage::GetPermanentBuffers() const
	{
		return m_permanentBuffers;
	}


	inline std::vector<std::shared_ptr<re::Buffer const>> const& RenderStage::GetPerFrameBuffers() const
	{
		return m_singleFrameBuffers;
	}


	inline std::vector<re::Batch> const& RenderStage::GetStageBatches() const
	{
		return m_stageBatches;
	}


	// We need to provide a destructor implementation since it's pure virtual
	inline RenderStage::IStageParams::~IStageParams() {}
	inline LibraryStage::IPayload::~IPayload() {};
}