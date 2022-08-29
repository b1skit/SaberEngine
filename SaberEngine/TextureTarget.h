#pragma once

#include <memory>

#include "DebugConfiguration.h"
#include "Texture.h"
#include "TextureTarget_Platform.h"


namespace gr
{
	// Wrapper for an individual render target texture
	class TextureTarget
	{
	public:
		TextureTarget();
		TextureTarget(std::shared_ptr<gr::Texture> texture);
		
		~TextureTarget();

		TextureTarget(TextureTarget&&) = default;

		TextureTarget& operator=(TextureTarget const&) = default;
		TextureTarget& operator=(std::shared_ptr<gr::Texture> texture);

		std::shared_ptr<gr::Texture>& GetTexture() { return m_texture; }
		std::shared_ptr<gr::Texture> const& GetTexture() const { return m_texture; }

		platform::TextureTarget::PlatformParams* const GetPlatformParams() { return m_platformParams.get(); }
		platform::TextureTarget::PlatformParams const* const GetPlatformParams() const { return m_platformParams.get(); }

	private:
		std::shared_ptr<gr::Texture> m_texture;
		std::shared_ptr<platform::TextureTarget::PlatformParams> m_platformParams;

		// Friends:
		friend bool platform::RegisterPlatformFunctions();
		friend void platform::TextureTarget::PlatformParams::CreatePlatformParams(gr::TextureTarget&);
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

		uint32_t& xMin() { return m_xMin; }
		uint32_t const& xMin() const{ return m_xMin; }

		uint32_t& yMin() { return m_yMin; }
		uint32_t const& yMin() const { return m_yMin; }

		uint32_t& Width() { return m_width; }
		uint32_t const& Width() const { return m_width; }

		uint32_t& Height() { return m_height; }
		uint32_t const& Height() const { return m_height; }

		
	private:
		// Viewport origin pixel coordinates. (0,0) by default
		uint32_t m_xMin;
		uint32_t m_yMin;

		// Viewport dimensions. Full window resolution by default
		uint32_t m_width;
		uint32_t m_height;
	};


	// Collection of render target textures
	class TextureTargetSet
	{
	public:
		TextureTargetSet();
		~TextureTargetSet();

		TextureTargetSet(TextureTargetSet const&) = default;
		TextureTargetSet(TextureTargetSet&&) = default;
		TextureTargetSet& operator=(TextureTargetSet const&) = default;

		std::vector<gr::TextureTarget>& ColorTargets() { m_targetStateDirty = true; return m_colorTargets; }
		std::vector<gr::TextureTarget> const& ColorTargets() const { return m_colorTargets; }

		gr::TextureTarget& ColorTarget(size_t i)
			{ SEAssert("OOB index", i < m_colorTargets.size()); m_targetStateDirty = true; return m_colorTargets[i]; }
		gr::TextureTarget const& ColorTarget(size_t i) const 
			{ SEAssert("OOB index", i < m_colorTargets.size()); return m_colorTargets[i]; }

		gr::TextureTarget& DepthStencilTarget() { m_targetStateDirty = true; return m_depthStencilTarget; }
		gr::TextureTarget const& DepthStencilTarget() const { return m_depthStencilTarget; }

		gr::Viewport& Viewport() { return m_viewport; }
		gr::Viewport const& Viewport() const { return m_viewport; }

		platform::TextureTargetSet::PlatformParams* const GetPlatformParams() { return m_platformParams.get(); }
		platform::TextureTargetSet::PlatformParams const* const GetPlatformParams() const { return m_platformParams.get(); }
		
		bool HasTargets();

		// Platform wrappers:
		void CreateColorTargets(uint32_t firstTextureUnit);
		void AttachColorTargets(uint32_t face, uint32_t mipLevel, bool doBind) const;

		void CreateDepthStencilTarget(uint32_t textureUnit);
		void AttachDepthStencilTarget(bool doBind) const;

		void CreateColorDepthStencilTargets(uint32_t firstColorTexUnit, uint32_t depthTexUnit);
		void AttachColorDepthStencilTargets(uint32_t colorFace, uint32_t colorMipLevel, bool doBind) const;
		
	private:
		std::vector<gr::TextureTarget> m_colorTargets;
		gr::TextureTarget m_depthStencilTarget;
		bool m_targetStateDirty;
		bool m_hasTargets;

		gr::Viewport m_viewport;

		std::shared_ptr<platform::TextureTargetSet::PlatformParams> m_platformParams;

		// Friends:
		friend bool platform::RegisterPlatformFunctions();
		friend void platform::TextureTargetSet::PlatformParams::CreatePlatformParams(gr::TextureTargetSet&);
	};
}
