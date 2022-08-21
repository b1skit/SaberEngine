#include "TextureTarget.h"
#include "CoreEngine.h"


namespace gr
{
	/**************/
	//TextureTarget
	/**************/
	gr::TextureTarget::TextureTarget() :
		m_texture(nullptr)
	{
		platform::TextureTarget::PlatformParams::CreatePlatformParams(*this);
	}


	gr::TextureTarget::TextureTarget(std::shared_ptr<gr::Texture> texture) :
		m_texture(texture)
	{
		platform::TextureTarget::PlatformParams::CreatePlatformParams(*this);
	}


	TextureTarget& TextureTarget::operator=(std::shared_ptr<gr::Texture> texture)
	{
		m_texture = texture;

		return *this;
	}


	TextureTarget::~TextureTarget()
	{
		m_texture = nullptr;
		m_platformParams = nullptr;
	}

	
	/**********/
	// Viewport
	/**********/
	Viewport::Viewport() :
		m_xMin(0),
		m_yMin(0),
		m_width(SaberEngine::CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowXRes")),
		m_height(SaberEngine::CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowYRes"))
	{

	}


	Viewport::Viewport(uint32_t xMin, uint32_t yMin, uint32_t width, uint32_t height) :
		m_xMin(xMin),
		m_yMin(yMin),
		m_width(width),
		m_height(height)
	{

	}

	/******************/
	// TextureTargetSet
	/******************/
	TextureTargetSet::TextureTargetSet()
	{
		platform::TextureTargetSet::PlatformParams::CreatePlatformParams(*this);
	
		m_colorTargets.resize(platform::TextureTargetSet::MaxColorTargets());
	}


	TextureTargetSet::~TextureTargetSet()
	{
		for (size_t i = 0; i < m_colorTargets.size(); i++)
		{
			m_colorTargets[i] = nullptr;
		}
		m_depthStencilTarget = nullptr;
		m_platformParams = nullptr;
	}


	void TextureTargetSet::CreateColorTargets(uint32_t firstTextureUnit)
	{
		platform::TextureTargetSet::CreateColorTargets(*this, firstTextureUnit);
	}


	void TextureTargetSet::AttachColorTargets(uint32_t face, uint32_t mipLevel, bool doBind) const
	{
		platform::TextureTargetSet::AttachColorTargets(*this, face, mipLevel, doBind);
	}


	void TextureTargetSet::CreateDepthStencilTarget(uint32_t textureUnit)
	{
		platform::TextureTargetSet::CreateDepthStencilTarget(*this, textureUnit);
	}


	void TextureTargetSet::AttachDepthStencilTarget(bool doBind) const
	{
		platform::TextureTargetSet::AttachDepthStencilTarget(*this, doBind);
	}


	void TextureTargetSet::CreateColorDepthStencilTargets(uint32_t firstColorTexUnit, uint32_t depthTexUnit)
	{
		CreateColorTargets(firstColorTexUnit);
		CreateDepthStencilTarget(depthTexUnit);
	}


	void TextureTargetSet::AttachColorDepthStencilTargets(uint32_t colorFace, uint32_t colorMipLevel, bool doBind) const
	{
		AttachColorTargets(colorFace, colorMipLevel, doBind);
		AttachDepthStencilTarget(doBind);
	}
}