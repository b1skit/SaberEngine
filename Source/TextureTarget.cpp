// © 2022 Adam Badke. All rights reserved.
#include "Config.h"
#include "TextureTarget.h"
#include "TextureTarget_Platform.h"
#include "RenderManager.h"


namespace re
{
	using en::Config;
	using std::string;


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
		m_width(Config::Get()->GetValue<int>(en::Config::k_windowXResValueName)),
		m_height(Config::Get()->GetValue<int>(en::Config::k_windowYResValueName))
	{
	}


	Viewport::Viewport(uint32_t xMin, uint32_t yMin, uint32_t width, uint32_t height) :
		m_xMin(xMin),
		m_yMin(yMin),
		m_width(width),
		m_height(height)
	{
	}

	/************/
	// ScissorRect
	/************/
	ScissorRect::ScissorRect()
		: m_left(0)
		, m_top(0)
		, m_right(std::numeric_limits<long>::max())
		, m_bottom(std::numeric_limits<long>::max())
	{
	}


	ScissorRect::ScissorRect(long left, long top, long right, long bottom)
		: m_left(left)
		, m_top(top)
		, m_right(right)
		, m_bottom(bottom)
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
	
		m_colorTargets.resize(platform::Context::GetMaxColorTargets());

		m_targetParameterBlock = re::ParameterBlock::Create(
			"RenderTargetParams",
			TargetParams(), // Defaults for now
			re::ParameterBlock::PBType::Mutable);
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

		m_targetParameterBlock = re::ParameterBlock::Create(
			"RenderTargetParams",
			TargetParams(), // Defaults for now
			re::ParameterBlock::PBType::Mutable);
	}


	TextureTargetSet& TextureTargetSet::operator=(TextureTargetSet const& rhs)
	{
		if (this == &rhs)
		{
			return *this;
		}

		SetName(rhs.GetName());

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


	TextureTargetSet::~TextureTargetSet()
	{
		for (uint8_t slot = 0; slot < m_colorTargets.size(); slot++)
		{
			m_colorTargets[slot] = nullptr;
		}
		m_depthStencilTarget = nullptr;
		m_platformParams = nullptr;

		m_colorTargetStateDirty = true;
		m_hasColorTarget = false;

		m_targetParameterBlock = nullptr;
		m_targetParamsDirty = true;
	}


	re::TextureTarget const& TextureTargetSet::GetColorTarget(uint8_t slot) const
	{
		SEAssert("OOB index", slot < m_colorTargets.size()); 
		return m_colorTargets[slot];
	}


	void TextureTargetSet::SetColorTarget(uint8_t slot, re::TextureTarget texTarget)
	{
		m_colorTargets[slot] = texTarget;
		m_colorTargetStateDirty = true;
		m_targetParamsDirty = true;
	}


	void TextureTargetSet::SetColorTarget(uint8_t slot, std::shared_ptr<re::Texture> texTarget)
	{
		m_colorTargets[slot] = texTarget;
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
		for (uint8_t slot = 0; slot < m_colorTargets.size(); slot++)
		{
			if (m_colorTargets[slot].GetTexture() != nullptr)
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
		return GetDepthStencilTarget().GetTexture() != nullptr;
	}


	uint8_t TextureTargetSet::GetNumColorTargets() const
	{
		// TODO: Optimize this. We should track/update the state with the dirty flag. For now, just count.

		uint8_t numTargets = 0;
		for (re::TextureTarget const& target : m_colorTargets)
		{
			if (target.HasTexture())
			{
				numTargets++;
			}
		}
		return numTargets;
	}


	std::shared_ptr<re::ParameterBlock> TextureTargetSet::GetTargetParameterBlock()
	{
		UpdateTargetParameterBlock();
		return m_targetParameterBlock;
	}


	void TextureTargetSet::UpdateTargetParameterBlock()
	{
		if (m_targetParamsDirty)
		{
			glm::vec4 targetDimensions;
			bool foundDimensions = false;

			// Default framebuffer has no texture targets
			if (!HasTargets())
			{
				const uint32_t xRes = (uint32_t)Config::Get()->GetValue<int>(en::Config::k_windowXResValueName);
				const uint32_t yRes = (uint32_t)Config::Get()->GetValue<int>(en::Config::k_windowYResValueName);

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
				for (uint8_t slot = 0; slot < m_colorTargets.size(); slot++)
				{
					std::shared_ptr<re::Texture> texTarget = m_colorTargets[slot].GetTexture();
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

			m_targetParameterBlock->Commit(targetParams);

			m_targetParamsDirty = false;
		}
	}
}
