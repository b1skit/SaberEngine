#include "TextureTarget.h"
#include "Config.h"

using en::Config;
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
		m_width(Config::Get()->GetValue<int>("windowXRes")),
		m_height(Config::Get()->GetValue<int>("windowYRes"))
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
			NamedObject(name),
		m_targetStateDirty(true),
		m_hasTargets(false),
		m_targetParameterBlock(nullptr),
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
			NamedObject(newName),
		m_colorTargets(rhs.m_colorTargets),
		m_depthStencilTarget(rhs.m_depthStencilTarget),
		m_targetStateDirty(true),
		m_hasTargets(rhs.m_hasTargets),
		m_viewport(rhs.m_viewport),
		m_platformParams(nullptr), // Targets are copied, but the target set must be created
		m_targetParameterBlock(rhs.m_targetParameterBlock),
		m_colorIsCreated(false),
		m_depthIsCreated(false)
	{
		platform::TextureTargetSet::PlatformParams::CreatePlatformParams(*this);
	}


	TextureTargetSet& TextureTargetSet::operator=(TextureTargetSet const& rhs)
	{
		if (this == &rhs)
		{
			return *this;
		}

		*const_cast<std::string*>(&GetName()) = rhs.GetName();

		m_colorTargets = rhs.m_colorTargets;
		m_depthStencilTarget = rhs.m_depthStencilTarget;
		m_targetStateDirty = rhs.m_targetStateDirty;
		m_hasTargets = rhs.m_hasTargets;
		m_viewport = rhs.m_viewport;
		m_platformParams = rhs.m_platformParams;
		m_targetParameterBlock = rhs.m_targetParameterBlock;
		m_colorIsCreated = rhs.m_colorIsCreated;
		m_depthIsCreated = rhs.m_depthIsCreated;

		return *this;
	}


	void TextureTargetSet::CreateColorTargets()
	{
		SEAssert("Texture Target Set already created!", m_colorIsCreated == false);
		m_colorIsCreated = true;
		platform::TextureTargetSet::CreateColorTargets(*this);
		CreateUpdateTargetParameterBlock();
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
		CreateUpdateTargetParameterBlock();
	}


	void TextureTargetSet::AttachDepthStencilTarget(bool doBind) const
	{
		platform::TextureTargetSet::AttachDepthStencilTarget(*this, doBind);
	}


	void TextureTargetSet::CreateColorDepthStencilTargets()
	{
		CreateColorTargets();
		CreateDepthStencilTarget();
		// Note: Calling both of these results in the param block being created and updated in the same call. This 
		// shouldn't be a problem; it's just a little wasteful
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


	void TextureTargetSet::CreateUpdateTargetParameterBlock()
	{
		glm::vec4 targetDimensions;
		bool foundDimensions = false;

		// Default framebuffer has no texture targets
		if (!HasTargets())
		{
			const uint32_t xRes = (uint32_t)Config::Get()->GetValue<int>("windowXRes");
			const uint32_t yRes = (uint32_t)Config::Get()->GetValue<int>("windowYRes");

			targetDimensions.x = (float)xRes;
			targetDimensions.y = (float)yRes;
			targetDimensions.z = 1.0f / xRes;
			targetDimensions.w = 1.0f / yRes;

			foundDimensions = true;
		}		

		// Find a single target we can get the resolution details from; This assumes all targets are the same dimensions
		if (m_depthIsCreated && !foundDimensions)
		{
			std::shared_ptr<gr::Texture> depthTarget = m_depthStencilTarget.GetTexture();
			if (depthTarget)
			{
				targetDimensions = depthTarget->GetTextureDimenions();
				foundDimensions = true;
			}
		}

		if (m_colorIsCreated && !foundDimensions)
		{
			for (size_t i = 0; i < m_colorTargets.size(); i++)
			{
				std::shared_ptr<gr::Texture> texTarget = m_colorTargets[i].GetTexture();
				if (texTarget)
				{
					targetDimensions = texTarget->GetTextureDimenions();
					foundDimensions = true;
					break;
				}
			}
		}

		SEAssert("Cannot create parameter block with no texture dimensions", foundDimensions);

		TargetParams targetParams;
		targetParams.g_targetResolution = targetDimensions;

		// Create the PB if required, or update it otherwise
		if (m_targetParameterBlock == nullptr)
		{
			m_targetParameterBlock = re::ParameterBlock::Create(
				"RenderTargetParams", 
				targetParams,
				re::ParameterBlock::UpdateType::Mutable, 
				re::ParameterBlock::Lifetime::Permanent);
		}
		else
		{
			m_targetParameterBlock->Commit(targetParams);
		}
	}
}
