#include "TextureTarget.h"
#include "TextureTarget_Platform.h"
#include "Config.h"

using en::Config;
using std::string;


namespace re
{
	/**************/
	//TextureTarget
	/**************/
	TextureTarget::TextureTarget() :
		m_texture(nullptr)
	{
		platform::TextureTarget::CreatePlatformParams(*this);
	}


	TextureTarget::TextureTarget(std::shared_ptr<re::Texture> texture) :
		m_texture(texture)
	{
		platform::TextureTarget::CreatePlatformParams(*this);
	}


	TextureTarget& TextureTarget::operator=(std::shared_ptr<re::Texture> texture)
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
	TextureTargetSet::TextureTargetSet(string name)
		: NamedObject(name)
		, m_colorTargetStateDirty(true)
		, m_hasColorTarget(false)
		, m_targetParameterBlock(nullptr)
		, m_targetParamsDirty(true)
	{
		platform::TextureTargetSet::CreatePlatformParams(*this);
	
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

		m_colorTargetStateDirty = true;
		m_hasColorTarget = false;

		m_targetParameterBlock = nullptr;
		m_targetParamsDirty = true;
	}


	TextureTargetSet::TextureTargetSet(TextureTargetSet const& rhs, std::string const& newName)
		: NamedObject(newName)
		, m_colorTargets(rhs.m_colorTargets)
		, m_depthStencilTarget(rhs.m_depthStencilTarget)
		, m_colorTargetStateDirty(true)
		, m_hasColorTarget(rhs.m_hasColorTarget)
		, m_viewport(rhs.m_viewport)
		, m_platformParams(nullptr) // Targets are copied, but the target set must be created
		, m_targetParameterBlock(rhs.m_targetParameterBlock)
		, m_targetParamsDirty(rhs.m_targetParamsDirty)
	{
		platform::TextureTargetSet::CreatePlatformParams(*this);
	}


	TextureTargetSet& TextureTargetSet::operator=(TextureTargetSet const& rhs)
	{
		if (this == &rhs)
		{
			return *this;
		}

		m_name = rhs.m_name;
		m_nameID = rhs.m_nameID;

		m_colorTargets = rhs.m_colorTargets;
		m_depthStencilTarget = rhs.m_depthStencilTarget;
		m_colorTargetStateDirty = rhs.m_colorTargetStateDirty;
		m_hasColorTarget = rhs.m_hasColorTarget;
		m_viewport = rhs.m_viewport;
		m_platformParams = rhs.m_platformParams;
		m_targetParameterBlock = rhs.m_targetParameterBlock;
		m_targetParamsDirty = rhs.m_targetParamsDirty;

		return *this;
	}


	re::TextureTarget const& TextureTargetSet::GetColorTarget(size_t i) const
	{
		SEAssert("OOB index", i < m_colorTargets.size()); 
		return m_colorTargets[i];
	}


	void TextureTargetSet::SetColorTarget(size_t i, re::TextureTarget texTarget)
	{
		m_colorTargets[i] = texTarget;
		m_colorTargetStateDirty = true;
		m_targetParamsDirty = true;
	}


	void TextureTargetSet::SetColorTarget(size_t i, std::shared_ptr<re::Texture> texTarget)
	{
		m_colorTargets[i] = texTarget;
		m_colorTargetStateDirty = true;
		m_targetParamsDirty = true;
	}


	void TextureTargetSet::SetDepthStencilTarget(re::TextureTarget const& depthStencilTarget)
	{
		m_depthStencilTarget = depthStencilTarget;
		m_targetParamsDirty = true;
	}


	void TextureTargetSet::SetDepthStencilTarget(std::shared_ptr<re::Texture> depthStencilTarget)
	{
		m_depthStencilTarget = depthStencilTarget;
		m_targetParamsDirty = true;
	}


	bool TextureTargetSet::HasTargets()
	{
		return (HasDepthTarget() || HasColorTarget());
	}


	bool TextureTargetSet::HasColorTarget()
	{
		if (!m_colorTargetStateDirty)
		{
			return m_hasColorTarget;
		}

		// If the state is dirty, we need to recheck:
		m_hasColorTarget = false;
		for (size_t i = 0; i < m_colorTargets.size(); i++)
		{
			if (m_colorTargets[i].GetTexture() != nullptr)
			{
				m_hasColorTarget = true;
				break;
			}
		}
		m_colorTargetStateDirty = false;

		return m_hasColorTarget;
	}


	bool TextureTargetSet::HasDepthTarget()
	{
		return DepthStencilTarget().GetTexture() != nullptr;
	}


	std::shared_ptr<re::ParameterBlock> TextureTargetSet::GetTargetParameterBlock()
	{
		CreateUpdateTargetParameterBlock();
		return m_targetParameterBlock;
	}


	void TextureTargetSet::CreateUpdateTargetParameterBlock()
	{
		if (m_targetParamsDirty)
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
			if (!foundDimensions && HasDepthTarget())
			{
				std::shared_ptr<re::Texture> depthTarget = m_depthStencilTarget.GetTexture();
				if (depthTarget)
				{
					targetDimensions = depthTarget->GetTextureDimenions();
					foundDimensions = true;
				}
			}

			if (!foundDimensions && HasColorTarget())
			{
				for (size_t i = 0; i < m_colorTargets.size(); i++)
				{
					std::shared_ptr<re::Texture> texTarget = m_colorTargets[i].GetTexture();
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

			m_targetParamsDirty = false;
		}
	}
}
