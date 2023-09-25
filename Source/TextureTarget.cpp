// © 2022 Adam Badke. All rights reserved.
#include "CastUtils.h"
#include "Config.h"
#include "SysInfo_Platform.h"
#include "TextureTarget.h"
#include "TextureTarget_Platform.h"
#include "RenderManager.h"

using en::Config;
using std::string;


namespace re
{
	/**************/
	//TextureTarget
	/**************/

	TextureTarget::TextureTarget(std::shared_ptr<re::Texture> texture, TargetParams const& targetParams)
		: m_texture(texture)
	{
		platform::TextureTarget::CreatePlatformParams(*this);
		m_targetParams = targetParams;
	}


	TextureTarget::~TextureTarget()
	{
		m_texture = nullptr;
		m_platformParams = nullptr;
		m_targetParams = {};
	}


	TextureTarget::TextureTarget(TextureTarget const& rhs)
	{
		*this = rhs;
	}


	TextureTarget& TextureTarget::operator=(TextureTarget const& rhs)
	{
		if (&rhs == this)
		{
			return *this;
		}

		m_texture = rhs.m_texture;
		platform::TextureTarget::CreatePlatformParams(*this);
		m_targetParams = rhs.m_targetParams;

		return *this;
	}


	void TextureTarget::SetTargetParams(TargetParams const& targetParams)
	{
		SEAssert("There are more than 6 faces specified. This is unexpected",
			targetParams.m_targetFace < 6);
		SEAssert("It is invalid to target all mips, only a single mip level can be specified", 
			targetParams.m_targetMip != re::Texture::k_allMips);

		m_targetParams = targetParams;
	}


	void TextureTarget::SetBlendMode(TargetParams::BlendModes const& blendModes)
	{
		m_targetParams.m_blendModes = blendModes;
	}


	re::TextureTarget::TargetParams::BlendModes const& TextureTarget::GetBlendMode() const
	{
		return m_targetParams.m_blendModes;
	}


	void TextureTarget::SetColorWriteMode(TargetParams::ChannelWrite const& colorWriteMode)
	{
		m_targetParams.m_channelWriteMode = colorWriteMode;
	}


	TextureTarget::TargetParams::ChannelWrite const& TextureTarget::GetColorWriteMode() const
	{
		return m_targetParams.m_channelWriteMode;
	};


	bool TextureTarget::WritesColor() const
	{
		return m_texture != nullptr &&
			(m_targetParams.m_channelWriteMode.R ||
				m_targetParams.m_channelWriteMode.G ||
				m_targetParams.m_channelWriteMode.B ||
				m_targetParams.m_channelWriteMode.A);
	}


	void TextureTarget::SetDepthWriteMode(TextureTarget::TargetParams::ChannelWrite::Mode depthWriteMode)
	{
		m_targetParams.m_channelWriteMode.R = depthWriteMode;

		// Disable the other channels for this target:
		m_targetParams.m_channelWriteMode.G = TextureTarget::TargetParams::ChannelWrite::Mode::Disabled;
		m_targetParams.m_channelWriteMode.B = TextureTarget::TargetParams::ChannelWrite::Mode::Disabled;
		m_targetParams.m_channelWriteMode.A = TextureTarget::TargetParams::ChannelWrite::Mode::Disabled;
	}


	TextureTarget::TargetParams::ChannelWrite::Mode TextureTarget::GetDepthWriteMode() const
	{
		return m_targetParams.m_channelWriteMode.R;
	}


	/**********/
	// Viewport
	/**********/
	Viewport::Viewport() :
		m_xMin(0),
		m_yMin(0),
		m_width(Config::Get()->GetValue<int>(en::ConfigKeys::k_windowXResValueName)),
		m_height(Config::Get()->GetValue<int>(en::ConfigKeys::k_windowYResValueName))
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
		, m_right(en::Config::Get()->GetValue<int>(en::ConfigKeys::k_windowXResValueName))
		, m_bottom(en::Config::Get()->GetValue<int>(en::ConfigKeys::k_windowYResValueName))
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

	std::shared_ptr<re::TextureTargetSet> TextureTargetSet::Create(std::string const& name)
	{
		std::shared_ptr<re::TextureTargetSet> newTextureTargetSet = nullptr;
		newTextureTargetSet.reset(new re::TextureTargetSet(name));

		re::RenderManager::Get()->RegisterForCreate(newTextureTargetSet);

		return newTextureTargetSet;
	}


	std::shared_ptr<re::TextureTargetSet> TextureTargetSet::Create(TextureTargetSet const& rhs, std::string const& name)
	{
		std::shared_ptr<re::TextureTargetSet> newTextureTargetSet = nullptr;
		newTextureTargetSet.reset(new re::TextureTargetSet(rhs, name));

		re::RenderManager::Get()->RegisterForCreate(newTextureTargetSet);

		return newTextureTargetSet;
	}


	TextureTargetSet::TextureTargetSet(string const& name)
		: NamedObject(name)
		, m_numColorTargets(0)
	{
		platform::TextureTargetSet::CreatePlatformParams(*this);

		m_colorTargets.resize(platform::SysInfo::GetMaxRenderTargets());
	}


	TextureTargetSet::TextureTargetSet(TextureTargetSet const& rhs, std::string const& newName)
		: NamedObject(newName)
		, m_numColorTargets(rhs.m_numColorTargets)
		, m_viewport(rhs.m_viewport)
		, m_platformParams(nullptr) // Targets are copied, but the target set must be created
	{
		platform::TextureTargetSet::CreatePlatformParams(*this);

		m_colorTargets.resize(platform::SysInfo::GetMaxRenderTargets());

		for (size_t i = 0; i < platform::SysInfo::GetMaxRenderTargets(); i++)
		{
			m_colorTargets[i] = rhs.m_colorTargets[i];
		}
		m_depthStencilTarget = rhs.m_depthStencilTarget;
	}


	TextureTargetSet::~TextureTargetSet()
	{
		m_colorTargets.clear();
		m_depthStencilTarget = re::TextureTarget();
		m_platformParams = nullptr;

		m_numColorTargets = 0;
	}


	re::TextureTarget const& TextureTargetSet::GetColorTarget(uint8_t slot) const
	{
		SEAssert("OOB index", slot < m_colorTargets.size());
		return m_colorTargets[slot];
	}


	void TextureTargetSet::SetColorTarget(uint8_t slot, re::TextureTarget const& texTarget)
	{
		SEAssert("Target sets are immutable after they've been committed", !m_platformParams->m_isCommitted);
		SEAssert("Targets must be set in monotonically-increasing order", 
			slot == 0 || m_colorTargets[slot - 1].HasTexture());
		m_colorTargets[slot] = texTarget;
	}


	void TextureTargetSet::SetColorTarget(
		uint8_t slot, std::shared_ptr<re::Texture> texture, TextureTarget::TargetParams const& targetParams)
	{
		SEAssert("Target sets are immutable after they've been committed", !m_platformParams->m_isCommitted);
		SEAssert("Targets must be set in monotonically-increasing order",
			slot == 0 || m_colorTargets[slot - 1].HasTexture());
		m_colorTargets[slot] = re::TextureTarget(texture, targetParams);
	}


	re::TextureTarget const* TextureTargetSet::GetDepthStencilTarget() const
	{
		if (m_depthStencilTarget.HasTexture())
		{
			return &m_depthStencilTarget;
		}
		return nullptr;
	}


	void TextureTargetSet::SetDepthStencilTarget(re::TextureTarget const* depthStencilTarget)
	{
		SEAssert("Cannot set a null target", depthStencilTarget);
		SEAssert("Target sets are immutable after they've been created", !m_platformParams->m_isCommitted);
		m_depthStencilTarget = re::TextureTarget(*depthStencilTarget);
	}


	void TextureTargetSet::SetDepthStencilTarget(
		std::shared_ptr<re::Texture> depthStencilTarget, re::TextureTarget::TargetParams const& targetParams)
	{
		SEAssert("Target sets are immutable after they've been created", !m_platformParams->m_isCommitted);
		m_depthStencilTarget = re::TextureTarget(depthStencilTarget, targetParams);
	}


	void TextureTargetSet::SetDepthWriteMode(TextureTarget::TargetParams::ChannelWrite::Mode depthWriteMode)
	{
		m_depthStencilTarget.SetDepthWriteMode(depthWriteMode);
	}


	bool TextureTargetSet::HasTargets() const
	{
		return (HasDepthTarget() || HasColorTarget());
	}


	bool TextureTargetSet::HasColorTarget() const
	{
		return m_numColorTargets > 0;
	}


	bool TextureTargetSet::HasDepthTarget() const
	{
		return GetDepthStencilTarget() != nullptr;
	}
	

	void TextureTargetSet::SetAllColorWriteModes(TextureTarget::TargetParams::ChannelWrite const& colorWriteMode)
	{
		for (size_t i = 0; i < m_colorTargets.size(); i++)
		{
			m_colorTargets[i].SetColorWriteMode(colorWriteMode);
		}
	}


	bool TextureTargetSet::WritesColor() const
	{
		for (size_t i = 0; i < m_colorTargets.size(); i++)
		{
			if (m_colorTargets[i].WritesColor())
			{
				return true;
			}
		}
		return false;
	}


	uint8_t TextureTargetSet::GetNumColorTargets() const
	{
		return m_numColorTargets;
	}


	glm::vec4 TextureTargetSet::GetTargetDimensions() const
	{
		glm::vec4 targetDimensions(0.f);

		bool foundDimensions = false;

		// Find a single target we can get the resolution details from; This assumes all targets are the same dimensions
		if (m_depthStencilTarget.HasTexture())
		{
			std::shared_ptr<re::Texture> depthTargetTex = m_depthStencilTarget.GetTexture();
			targetDimensions = depthTargetTex->GetTextureDimenions();
			foundDimensions = true;
		}
		else
		{
			for (uint8_t slot = 0; slot < m_colorTargets.size(); slot++)
			{
				if (m_colorTargets[slot].HasTexture())
				{
					std::shared_ptr<re::Texture> colorTargetTex = m_colorTargets[slot].GetTexture();
					targetDimensions = colorTargetTex->GetTextureDimenions();
					foundDimensions = true;
					break;
				}
			}
		}

		// Default framebuffer has no texture targets
		// TODO: A default framebuffer target set should be identified by a flag; We shouldn't be implying it by emptiness
		// -> OR: A target has a flag (and just no texture resource, for OpenGL)?
		if (!foundDimensions)
		{
			const uint32_t xRes = (uint32_t)Config::Get()->GetValue<int>(en::ConfigKeys::k_windowXResValueName);
			const uint32_t yRes = (uint32_t)Config::Get()->GetValue<int>(en::ConfigKeys::k_windowYResValueName);

			targetDimensions.x = (float)xRes;
			targetDimensions.y = (float)yRes;
			targetDimensions.z = 1.0f / xRes;
			targetDimensions.w = 1.0f / yRes;

			foundDimensions = true;
		}

		return targetDimensions;
	}


	void TextureTargetSet::SetColorTargetBlendModes(
		size_t numTargets, 
		re::TextureTarget::TargetParams::BlendModes const* blendModesArray)
	{
		SEAssert("Array cannot be null", blendModesArray);
		SEAssert("Too many blend modes supplied", numTargets < m_colorTargets.size());

		for (size_t i = 0; i < numTargets; i++)
		{
			// Note: It's valid to set a blend mode even if a Target does not have a Texture
			m_colorTargets[i].SetBlendMode(blendModesArray[i]);
		}
	}


	void TextureTargetSet::SetAllColorTargetBlendModes(re::TextureTarget::TargetParams::BlendModes const& blendModes)
	{
		for (size_t i = 0; i < m_colorTargets.size(); i++)
		{
			// Note: It's valid to set a blend mode even if a Target does not have a Texture
			m_colorTargets[i].SetBlendMode(blendModes);
		}
	}


	void TextureTargetSet::SetViewport(re::Viewport const& viewport)
	{
		m_viewport = viewport;
	}


	void TextureTargetSet::SetScissorRect(re::ScissorRect const& scissorRect)
	{
		SEAssert("Scissor rectangle is out of bounds of the viewport", 
			util::CheckedCast<uint32_t>(scissorRect.Left()) >= m_viewport.xMin() &&
			util::CheckedCast<uint32_t>(scissorRect.Top()) >= m_viewport.yMin() &&
			util::CheckedCast<uint32_t>(scissorRect.Right()) <= m_viewport.Width()&&
			util::CheckedCast<uint32_t>(scissorRect.Bottom()) <= m_viewport.Height());

		m_scissorRect = scissorRect;
	}


	void TextureTargetSet::RecomputeNumColorTargets()
	{
		SEAssert("Target sets are immutable after they've been committed", !m_platformParams->m_isCommitted);

		// Walk through and check each color target:
		m_numColorTargets = 0;
		for (uint8_t slot = 0; slot < m_colorTargets.size(); slot++)
		{
			if (m_colorTargets[slot].HasTexture())
			{
				m_numColorTargets++;
			}
		}
	}


	void TextureTargetSet::Commit()
	{
		SEAssert("Target sets are immutable after they've been committed", 
			!m_platformParams->m_isCommitted);

		RecomputeNumColorTargets();
		ComputeDataHash();

		m_platformParams->m_isCommitted = true;
	}


	void TextureTargetSet::ComputeDataHash()
	{
		ResetDataHash();
		
		for (uint8_t slot = 0; slot < m_colorTargets.size(); slot++)
		{
			if (m_colorTargets[slot].HasTexture())
			{
				AddDataBytesToHash(m_colorTargets[slot].GetTexture()->GetTextureParams().m_format);
				AddDataBytesToHash(m_colorTargets[slot].GetBlendMode());
				AddDataBytesToHash(m_colorTargets[slot].GetColorWriteMode());
			}
		}
		if (HasDepthTarget())
		{
			AddDataBytesToHash(m_depthStencilTarget.GetTexture()->GetTextureParams().m_format);
			AddDataBytesToHash(m_depthStencilTarget.GetDepthWriteMode());
		}		
	}


	uint64_t TextureTargetSet::GetTargetSetSignature()
	{
		Commit();
		return GetDataHash();
	}


	uint64_t TextureTargetSet::GetTargetSetSignature() const
	{
		SEAssert("Trying to get the signature, but the targets haven't been committed",
			HasTargets() && m_platformParams->m_isCommitted);

		return GetDataHash();
	}
}
