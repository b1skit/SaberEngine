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
		SEAssert(targetParams.m_targetFace < 6, "There are more than 6 faces specified. This is unexpected");
		SEAssert(targetParams.m_targetMip != re::Texture::k_allMips,
			"It is invalid to target all mips, only a single mip level can be specified");

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


	void TextureTarget::SetClearMode(re::TextureTarget::TargetParams::ClearMode clearMode)
	{
		m_targetParams.m_clearMode = clearMode;
	}


	re::TextureTarget::TargetParams::ClearMode TextureTarget::GetClearMode() const
	{
		return m_targetParams.m_clearMode;
	}


	/**********/
	// Viewport
	/**********/
	Viewport::Viewport() :
		m_xMin(0),
		m_yMin(0),
		m_width(Config::Get()->GetValue<int>(en::ConfigKeys::k_windowWidthKey)),
		m_height(Config::Get()->GetValue<int>(en::ConfigKeys::k_windowHeightKey))
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
		, m_right(en::Config::Get()->GetValue<int>(en::ConfigKeys::k_windowWidthKey))
		, m_bottom(en::Config::Get()->GetValue<int>(en::ConfigKeys::k_windowHeightKey))
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
		SEAssert(slot < m_colorTargets.size(), "OOB index");
		return m_colorTargets[slot];
	}


	void TextureTargetSet::SetColorTarget(uint8_t slot, re::TextureTarget const& texTarget)
	{
		SEAssert(!m_platformParams->m_isCommitted, "Target sets are immutable after they've been committed");
		SEAssert(slot == 0 || m_colorTargets[slot - 1].HasTexture(), 
			"Targets must be set in monotonically-increasing order");
		m_colorTargets[slot] = texTarget;
	}


	void TextureTargetSet::SetColorTarget(
		uint8_t slot, std::shared_ptr<re::Texture> texture, TextureTarget::TargetParams const& targetParams)
	{
		SEAssert(!m_platformParams->m_isCommitted, "Target sets are immutable after they've been committed");
		SEAssert(slot == 0 || m_colorTargets[slot - 1].HasTexture(),
			"Targets must be set in monotonically-increasing order");
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
		SEAssert(depthStencilTarget, "Cannot set a null target");
		SEAssert(!m_platformParams->m_isCommitted, "Target sets are immutable after they've been created");
		m_depthStencilTarget = re::TextureTarget(*depthStencilTarget);
	}


	void TextureTargetSet::SetDepthStencilTarget(
		std::shared_ptr<re::Texture> depthStencilTarget, re::TextureTarget::TargetParams const& targetParams)
	{
		SEAssert(!m_platformParams->m_isCommitted, "Target sets are immutable after they've been created");
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
			const uint32_t xRes = (uint32_t)Config::Get()->GetValue<int>(en::ConfigKeys::k_windowWidthKey);
			const uint32_t yRes = (uint32_t)Config::Get()->GetValue<int>(en::ConfigKeys::k_windowHeightKey);

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
		SEAssert(blendModesArray, "Array cannot be null");
		SEAssert(numTargets < m_colorTargets.size(), "Too many blend modes supplied");

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


	void TextureTargetSet::SetColorTargetClearMode(size_t targetIdx, re::TextureTarget::TargetParams::ClearMode clearMode)
	{
		m_colorTargets[targetIdx].SetClearMode(clearMode);
	}


	void TextureTargetSet::SetAllColorTargetClearModes(re::TextureTarget::TargetParams::ClearMode clearMode)
	{
		for (size_t i = 0; i < m_colorTargets.size(); i++)
		{
			// Note: It's valid to set a clear mode even if a Target does not have a Texture
			m_colorTargets[i].SetClearMode(clearMode);
		}
	}


	void TextureTargetSet::SetDepthTargetClearMode(re::TextureTarget::TargetParams::ClearMode clearMode)
	{
		m_depthStencilTarget.SetClearMode(clearMode);
	}


	void TextureTargetSet::SetAllTargetClearModes(re::TextureTarget::TargetParams::ClearMode clearMode)
	{
		SetAllColorTargetClearModes(clearMode);
		SetDepthTargetClearMode(clearMode);
	}


	void TextureTargetSet::SetViewport(re::Viewport const& viewport)
	{
		m_viewport = viewport;
	}


	void TextureTargetSet::SetScissorRect(re::ScissorRect const& scissorRect)
	{
		SEAssert(util::CheckedCast<uint32_t>(scissorRect.Left()) >= m_viewport.xMin() &&
			util::CheckedCast<uint32_t>(scissorRect.Top()) >= m_viewport.yMin() &&
			util::CheckedCast<uint32_t>(scissorRect.Right()) <= m_viewport.Width()&&
			util::CheckedCast<uint32_t>(scissorRect.Bottom()) <= m_viewport.Height(),
			"Scissor rectangle is out of bounds of the viewport");

		m_scissorRect = scissorRect;
	}


	void TextureTargetSet::RecomputeNumColorTargets()
	{
		SEAssert(!m_platformParams->m_isCommitted, "Target sets are immutable after they've been committed");

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
		SEAssert(!m_platformParams->m_isCommitted,
			"Target sets are immutable after they've been committed");

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
		SEAssert(HasTargets() && m_platformParams->m_isCommitted,
			"Trying to get the signature, but the targets haven't been committed");

		return GetDataHash();
	}
}
