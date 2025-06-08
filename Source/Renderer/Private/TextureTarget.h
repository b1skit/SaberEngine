// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Private/BufferView.h"
#include "Private/Texture.h"
#include "Private/TextureView.h"

#include "Core/InvPtr.h"
#include "Core/Interfaces/IHashedDataObject.h"
#include "Core/Interfaces/IPlatformObject.h"
#include "Core/Interfaces/INamedObject.h"


struct TargetData;

namespace re
{
	// Wrapper for an individual render target texture
	class TextureTarget
	{
	public:
		struct PlatObj : public core::IPlatObj
		{
			virtual ~PlatObj() = default;

			bool m_isCreated = false; // Targets are immutable after creation
		};


	public:
		struct TargetParams
		{
			re::TextureView m_textureView;

			std::string m_shaderName; // For UAV targets
		};

	public:
		TextureTarget() = default;
		explicit TextureTarget(core::InvPtr<re::Texture> texture, TargetParams const&);
		
		~TextureTarget();

		TextureTarget(TextureTarget const&);
		TextureTarget& operator=(TextureTarget const&);

		TextureTarget(TextureTarget&&) noexcept = default;
		TextureTarget& operator=(TextureTarget&&) noexcept = default;

		inline bool HasTexture() const { return m_texture != nullptr; }

		core::InvPtr<re::Texture>& GetTexture() { return m_texture; }
		core::InvPtr<re::Texture> const& GetTexture() const { return m_texture; }

		void ReplaceTexture(core::InvPtr<re::Texture>, re::TextureView const&);

		void SetTargetParams(TargetParams const& targetParams);
		TargetParams const& GetTargetParams() const { return m_targetParams; }

		PlatObj* GetPlatformObject() const { return m_platObj.get(); }
		void SetPlatformObject(std::unique_ptr<PlatObj> platObj) { m_platObj = std::move(platObj); }


	private:
		core::InvPtr<re::Texture> m_texture;
		std::unique_ptr<PlatObj> m_platObj;

		TargetParams m_targetParams;
	};


	class Viewport
	{
	public:
		Viewport();
		Viewport(uint32_t xMin, uint32_t yMin, uint32_t width, uint32_t height);
		Viewport(core::InvPtr<re::Texture> const&); // Default Viewport about the entire texture

		Viewport(Viewport const&) = default;
		Viewport(Viewport&&) noexcept = default;
		Viewport& operator=(Viewport const&) = default;
		Viewport& operator=(Viewport&&) noexcept = default;
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
		ScissorRect(core::InvPtr<re::Texture> const&); // Default rectangle about the entire texture

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
		struct PlatObj : public core::IPlatObj
		{
			virtual ~PlatObj() = default;

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
		void SetColorTarget(uint8_t slot, core::InvPtr<re::Texture> const&); // Target MIP0
		void SetColorTarget(uint8_t slot, core::InvPtr<re::Texture> const&, TextureTarget::TargetParams const&);

		re::TextureTarget const& GetDepthStencilTarget() const;

		void SetDepthStencilTarget(re::TextureTarget const&);
		void SetDepthStencilTarget(core::InvPtr<re::Texture> const&); // Target MIP0
		void SetDepthStencilTarget(core::InvPtr<re::Texture> const&, re::TextureTarget::TargetParams const&);

		// Replace a TargetTexture with a pipeline-compatible alternative
		void ReplaceColorTargetTexture(uint8_t slot, core::InvPtr<re::Texture>&, re::TextureView const& texView);
		void ReplaceDepthStencilTargetTexture(core::InvPtr<re::Texture>, re::TextureView const&);

		bool HasTargets() const;
		bool HasColorTarget() const;
		bool HasDepthTarget() const;

		uint8_t GetNumColorTargets() const;
		glm::vec4 GetTargetDimensions() const;

		void SetViewport(re::Viewport const&);
		inline re::Viewport const& GetViewport() const { return m_viewport; }

		void SetScissorRect(re::ScissorRect const&);
		inline re::ScissorRect const& GetScissorRect() const { return m_scissorRect; }

		inline PlatObj* GetPlatformObject() const { return m_platObj.get(); }
		void SetPlatformObject(std::unique_ptr<PlatObj> platObj) { m_platObj = std::move(platObj); }
		
		// Commits and make immutable, then computes the data hash. Use this instead of IHashedDataObject::GetDataHash
		uint64_t GetTargetSetSignature(); 
		uint64_t GetTargetSetSignature() const;

		re::BufferInput const& GetCreateTargetParamsBuffer();


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

		std::unique_ptr<PlatObj> m_platObj;

		re::BufferInput m_targetParamsBuffer; // Only populated on demand


	private:
		TextureTargetSet() = delete;
		TextureTargetSet(TextureTargetSet const&) = delete;
		TextureTargetSet(TextureTargetSet&&) noexcept = delete;
		TextureTargetSet& operator=(TextureTargetSet const&) = delete;
		TextureTargetSet& operator=(TextureTargetSet&&) noexcept = delete;
	};
}
