// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Buffer.h"
#include "BufferInput.h"
#include "Texture.h"
#include "TextureView.h"

#include "Core/Assert.h"

#include "Core/Interfaces/IHashedDataObject.h"
#include "Core/Interfaces/IPlatformParams.h"
#include "Core/Interfaces/INamedObject.h"

struct TargetData;


namespace re
{
	// Wrapper for an individual render target texture
	class TextureTarget
	{
	public:
		struct PlatformParams : public core::IPlatformParams
		{
			virtual ~PlatformParams() = 0;

			bool m_isCreated = false; // Targets are immutable after creation
		};


	public:
		enum class BlendMode : uint8_t // Graphics stages only
		{
			Disabled,
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


		enum Channel : uint8_t
		{
			R = 1 << 0,
			G = 1 << 1,
			B = 1 << 2,
			A = 1 << 3,

			RGB = (R | G | B),
			All = (R | G | B | A),
		};
		using ColorWriteMask = uint8_t;


		enum class ClearMode : bool
		{
			Enabled,
			Disabled
		};


	public:
		struct TargetParams
		{
			re::TextureView m_textureView;

			std::string m_shaderName; // For UAV targets
			
			struct BlendModes
			{
				BlendMode m_srcBlendMode = BlendMode::One;
				BlendMode m_dstBlendMode = BlendMode::Zero;
			} m_blendModes;

			ColorWriteMask m_colorWriteMask = Channel::All;

			// TODO: Update PipelineState_DX12.cpp::BuildBlendDesc to have D3D12_BLEND_DESC::IndependentBlendEnable = true
			ClearMode m_clearMode = ClearMode::Disabled;

			// TODO: Support blend operations (add/subtract/min/max etc) for both color and alpha channels
			// TODO: We should support alpha blend modes, in addition to the color blend modes here
			// TODO: Support logical operations (AND/OR/XOR etc)
		};

	public:
		TextureTarget() = default;
		explicit TextureTarget(std::shared_ptr<re::Texture> texture, TargetParams const&);
		
		~TextureTarget();

		TextureTarget(TextureTarget const&);
		TextureTarget& operator=(TextureTarget const&);

		TextureTarget(TextureTarget&&) = default;		

		inline bool HasTexture() const { return m_texture != nullptr; }

		std::shared_ptr<re::Texture>& GetTexture() { return m_texture; }
		std::shared_ptr<re::Texture> const& GetTexture() const { return m_texture; }

		void ReplaceTexture(std::shared_ptr<re::Texture>, re::TextureView const&);

		void SetTargetParams(TargetParams const& targetParams);
		TargetParams const& GetTargetParams() const { return m_targetParams; }

		void SetBlendMode(TargetParams::BlendModes const&);
		TargetParams::BlendModes const& GetBlendMode() const;

		void SetColorWriteBit(Channel channels);
		ColorWriteMask GetColorWriteMask() const;
		bool WritesColor(Channel channel) const;
		bool WritesColor() const;

		void SetClearMode(re::TextureTarget::ClearMode);
		re::TextureTarget::ClearMode GetClearMode() const;

		PlatformParams* GetPlatformParams() const { return m_platformParams.get(); }
		void SetPlatformParams(std::unique_ptr<PlatformParams> params) { m_platformParams = std::move(params); }


	private:
		std::shared_ptr<re::Texture> m_texture;
		std::unique_ptr<PlatformParams> m_platformParams;

		TargetParams m_targetParams;
	};


	class Viewport
	{
	public:
		Viewport();
		Viewport(uint32_t xMin, uint32_t yMin, uint32_t width, uint32_t height);

		Viewport(Viewport const&) = default;
		Viewport(Viewport&&) = default;
		Viewport& operator=(Viewport const&) = default;
		~Viewport() = default;

		uint32_t& xMin() { return m_xMin; } // Top-left X
		uint32_t const xMin() const{ return m_xMin; }

		uint32_t& yMin() { return m_yMin; } // Top-left Y
		uint32_t const yMin() const { return m_yMin; }

		uint32_t& Width() { return m_width; }
		uint32_t const Width() const { return m_width; }

		uint32_t& Height() { return m_height; }
		uint32_t const Height() const { return m_height; }

		
	private:
		// Viewport origin pixel coordinates. (0,0) (top-left) by default
		uint32_t m_xMin;
		uint32_t m_yMin;

		// Viewport dimensions. Full window resolution by default
		uint32_t m_width;
		uint32_t m_height;

		// TODO: OpenGL expects ints, DX12 expects floats. We should support both (eg. via a union?)
	};


	class ScissorRect
	{
	public:
		ScissorRect();
		ScissorRect(long left, long top, long right, long bottom);

		long& Left() { return m_left; }
		long Left() const { return m_left; }

		long& Top() { return m_top; }
		long Top() const { return m_top; }

		long& Right() { return m_right; }
		long Right() const { return m_right; }

		long& Bottom() { return m_bottom; }
		long Bottom() const { return m_bottom; }

	private: // ScissorRect bounds, in pixel coordinates: 
		long m_left;	// Upper-left corner X coordinate
		long m_top;		// Upper-left corner Y coordinate
		long m_right;	// Width
		long m_bottom;	// Height
	};


	// Collection of render target textures
	class TextureTargetSet final : public core::INamedObject, public core::IHashedDataObject
	{
	public:
		struct PlatformParams : public core::IPlatformParams
		{
			virtual ~PlatformParams() = 0;

			bool m_isCommitted = false; // Target sets are immutable after commit
		};


	public:
		[[nodiscard]] static std::shared_ptr<re::TextureTargetSet> Create(std::string const& name);
		[[nodiscard]] static std::shared_ptr<re::TextureTargetSet> Create(TextureTargetSet const&, std::string const& name);

		// Copy an existing TextureTargetSet, but override the TargetParams
		[[nodiscard]] static std::shared_ptr<re::TextureTargetSet> Create(
			TextureTargetSet const&, re::TextureTarget::TargetParams const& overrideParams, char const* name);

		~TextureTargetSet();

		void Commit(); // Target sets are immutable after Commit: Called once during API creation

		inline std::vector<re::TextureTarget> const& GetColorTargets() const { return m_colorTargets; }
		re::TextureTarget const& GetColorTarget(uint8_t slot) const;

		// Color targets must be set in monotonically-increasing order from 0
		void SetColorTarget(uint8_t slot, re::TextureTarget const& texTarget);
		void SetColorTarget(uint8_t slot, std::shared_ptr<re::Texture> const&, TextureTarget::TargetParams const&);

		re::TextureTarget const& GetDepthStencilTarget() const;

		void SetDepthStencilTarget(re::TextureTarget const&);
		void SetDepthStencilTarget(std::shared_ptr<re::Texture> const&, re::TextureTarget::TargetParams const&);

		// Replace a TargetTexture with a pipeline-compatible alternative
		void ReplaceColorTargetTexture(uint8_t slot, std::shared_ptr<re::Texture>&, re::TextureView const& texView);
		void ReplaceDepthStencilTargetTexture(std::shared_ptr<re::Texture>, re::TextureView const&);

		bool HasTargets() const;
		bool HasColorTarget() const;
		bool HasDepthTarget() const;

		void SetAllColorWriteModes(TextureTarget::Channel channelMask);
		bool WritesColor() const;

		uint8_t GetNumColorTargets() const;
		glm::vec4 GetTargetDimensions() const;

		void SetColorTargetBlendModes(size_t numTargets, re::TextureTarget::TargetParams::BlendModes const* blendModesArray);
		void SetAllColorTargetBlendModes(re::TextureTarget::TargetParams::BlendModes const&);

		void SetColorTargetClearMode(size_t targetIdx, re::TextureTarget::ClearMode);
		void SetAllColorTargetClearModes(re::TextureTarget::ClearMode);
		void SetDepthTargetClearMode(re::TextureTarget::ClearMode);
		void SetAllTargetClearModes(re::TextureTarget::ClearMode);

		void SetViewport(re::Viewport const&);
		inline re::Viewport const& GetViewport() const { return m_viewport; }

		void SetScissorRect(re::ScissorRect const&);
		inline re::ScissorRect const& GetScissorRect() const { return m_scissorRect; }

		inline PlatformParams* GetPlatformParams() const { return m_platformParams.get(); }
		void SetPlatformParams(std::unique_ptr<PlatformParams> params) { m_platformParams = std::move(params); }
		
		// Commits and make immutable, then computes the data hash. Use this instead of IHashedDataObject::GetDataHash
		uint64_t GetTargetSetSignature(); 
		uint64_t GetTargetSetSignature() const;

		re::BufferInput const& GetCreateTargetParamsBuffer(re::Buffer::AllocationType = re::Buffer::AllocationType::Mutable);


	private: // Use the object Create factories instead
		explicit TextureTargetSet(std::string const& name);
		TextureTargetSet(TextureTargetSet const& rhs, std::string const& newName);


	private:
		void RecomputeNumColorTargets();
		void ComputeDataHash() override; // IHashedDataObject interface

		TargetData GetTargetParamsBufferData() const;

		void ValidateConfiguration() const; // _DEBUG only


	private:
		std::vector<re::TextureTarget> m_colorTargets; // == SysInfo::GetMaxRenderTargets() elements
		re::TextureTarget m_depthStencilTarget;

		uint8_t m_numColorTargets;

		re::Viewport m_viewport;
		re::ScissorRect m_scissorRect;

		std::unique_ptr<PlatformParams> m_platformParams;

		re::BufferInput m_targetParamsBuffer; // Only populated on demand


	private:
		TextureTargetSet() = delete;
		TextureTargetSet(TextureTargetSet const&) = delete;
		TextureTargetSet(TextureTargetSet&&) = delete;
		TextureTargetSet& operator=(TextureTargetSet const&) = delete;
		TextureTargetSet& operator=(TextureTargetSet&&) = delete;
	};


	// We need to provide a destructor implementation since it's pure virtual
	inline TextureTarget::PlatformParams::~PlatformParams() {};
	inline TextureTargetSet::PlatformParams::~PlatformParams() {};
}
