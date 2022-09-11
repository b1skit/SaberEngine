#include "TextureTarget.h"
#include "CoreEngine.h"

using std::string;


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
		m_width(en::CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowXRes")),
		m_height(en::CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowYRes"))
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
	TextureTargetSet::TextureTargetSet(string name) :
		m_name(name),
		m_targetStateDirty(true),
		m_hasTargets(false),
		m_colorIsCreated(false),
		m_depthIsCreated(false)
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

		m_targetStateDirty = true;
		m_hasTargets = false;
	}


	TextureTargetSet::TextureTargetSet(TextureTargetSet const& rhs, std::string const& newName) :
		m_name(newName),
		m_colorTargets(rhs.m_colorTargets),
		m_depthStencilTarget(rhs.m_depthStencilTarget),
		m_targetStateDirty(true),
		m_hasTargets(rhs.m_hasTargets),
		m_viewport(rhs.m_viewport),
		m_platformParams(nullptr), // Targets are copied, but the target set must be created
		m_colorIsCreated(false),
		m_depthIsCreated(false)
	{
		platform::TextureTargetSet::PlatformParams::CreatePlatformParams(*this);
	}

	//TextureTargetSet& TextureTargetSet::operator=(TextureTargetSet const& rhs)
	//{
	//	m_name = rhs.m_name + "_copy";
	//	m_colorTargets = rhs.m_colorTargets;
	//	m_depthStencilTarget = rhs.m_depthStencilTarget;
	//	m_targetStateDirty = true;
	//	m_hasTargets = rhs.m_hasTargets;
	//	m_viewport = rhs.m_viewport;
	//	m_platformParams = nullptr; // Targets are copied, but the target set must be created
	//	m_colorIsCreated = false;
	//	m_depthIsCreated = false;

	//	platform::TextureTargetSet::PlatformParams::CreatePlatformParams(*this);

	//	return *this;
	//}


	void TextureTargetSet::CreateColorTargets()
	{
		SEAssert("Texture Target Set already created!", m_colorIsCreated == false);
		m_colorIsCreated = true;
		platform::TextureTargetSet::CreateColorTargets(*this);
	}


	void TextureTargetSet::AttachColorTargets(uint32_t face, uint32_t mipLevel, bool doBind) const
	{
		platform::TextureTargetSet::AttachColorTargets(*this, face, mipLevel, doBind);
	}


	void TextureTargetSet::CreateDepthStencilTarget()
	{
		SEAssert("Texture Target Set already created!", m_depthIsCreated == false);
		m_depthIsCreated = true;
		platform::TextureTargetSet::CreateDepthStencilTarget(*this);
	}


	void TextureTargetSet::AttachDepthStencilTarget(bool doBind) const
	{
		platform::TextureTargetSet::AttachDepthStencilTarget(*this, doBind);
	}


	void TextureTargetSet::CreateColorDepthStencilTargets()
	{
		CreateColorTargets();
		CreateDepthStencilTarget();
	}


	void TextureTargetSet::AttachColorDepthStencilTargets(uint32_t colorFace, uint32_t colorMipLevel, bool doBind) const
	{
		AttachColorTargets(colorFace, colorMipLevel, doBind);
		AttachDepthStencilTarget(doBind);
	}


	bool TextureTargetSet::HasTargets()
	{
		if (!m_targetStateDirty)
		{
			return m_hasTargets;
		}

		m_hasTargets = DepthStencilTarget().GetTexture() != nullptr;

		for (size_t i = 0; i < m_colorTargets.size(); i++)
		{
			if (m_colorTargets[i].GetTexture() != nullptr)
			{
				m_hasTargets = true;
				break;
			}
		}

		m_targetStateDirty = false;

		return m_hasTargets;
	}
}
