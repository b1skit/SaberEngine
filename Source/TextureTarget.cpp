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
	TextureTargetSet::TextureTargetSet(string const& name)
		: NamedObject(name)
		, m_targetStateDirty(true)
		, m_numColorTargets(0)
		, m_targetParameterBlock(nullptr)
	{
		platform::TextureTargetSet::CreatePlatformParams(*this);
	
		m_colorTargets.resize(platform::Context::GetMaxColorTargets());

		m_targetParameterBlock = re::ParameterBlock::Create(
			RenderTargetParams::s_shaderName,
			RenderTargetParams(), // Defaults for now
			re::ParameterBlock::PBType::Mutable);
	}


	TextureTargetSet::TextureTargetSet(TextureTargetSet const& rhs, std::string const& newName)
		: NamedObject(newName)
		, m_colorTargets(rhs.m_colorTargets)
		, m_depthStencilTarget(rhs.m_depthStencilTarget)
		, m_targetStateDirty(true)
		, m_numColorTargets(rhs.m_numColorTargets)
		, m_viewport(rhs.m_viewport)
		, m_platformParams(nullptr) // Targets are copied, but the target set must be created
		, m_targetParameterBlock(rhs.m_targetParameterBlock)
	{
		platform::TextureTargetSet::CreatePlatformParams(*this);

		m_targetParameterBlock = re::ParameterBlock::Create(
			RenderTargetParams::s_shaderName,
			RenderTargetParams(), // Defaults for now
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
		m_targetStateDirty = rhs.m_targetStateDirty;
		m_numColorTargets = rhs.m_numColorTargets;
		m_viewport = rhs.m_viewport;
		m_platformParams = rhs.m_platformParams;
		m_targetParameterBlock = rhs.m_targetParameterBlock;

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

		m_targetStateDirty = true;
		m_numColorTargets = 0;

		m_targetParameterBlock = nullptr;
	}


	re::TextureTarget const& TextureTargetSet::GetColorTarget(uint8_t slot) const
	{
		SEAssert("OOB index", slot < m_colorTargets.size()); 
		return m_colorTargets[slot];
	}


	void TextureTargetSet::SetColorTarget(uint8_t slot, re::TextureTarget texTarget)
	{
		m_colorTargets[slot] = texTarget;
		m_targetStateDirty = true;
	}


	void TextureTargetSet::SetColorTarget(uint8_t slot, std::shared_ptr<re::Texture> texTarget)
	{
		m_colorTargets[slot] = texTarget;
		m_targetStateDirty = true;
	}


	void TextureTargetSet::SetDepthStencilTarget(re::TextureTarget const& depthStencilTarget)
	{
		m_depthStencilTarget = depthStencilTarget;
		m_targetStateDirty = true;
	}


	void TextureTargetSet::SetDepthStencilTarget(std::shared_ptr<re::Texture> depthStencilTarget)
	{
		m_depthStencilTarget = depthStencilTarget;
		m_targetStateDirty = true;
	}


	bool TextureTargetSet::HasTargets()
	{
		return (HasDepthTarget() || HasColorTarget());
	}


	bool TextureTargetSet::HasColorTarget()
	{
		RecomputeInternalState();
		return m_numColorTargets > 0;
	}


	bool TextureTargetSet::HasDepthTarget()
	{
		return GetDepthStencilTarget().GetTexture() != nullptr;
	}


	uint8_t TextureTargetSet::GetNumColorTargets()
	{
		RecomputeInternalState();
		return m_numColorTargets;
	}


	std::shared_ptr<re::ParameterBlock> TextureTargetSet::GetTargetParameterBlock()
	{
		RecomputeInternalState();
		return m_targetParameterBlock;
	}


	void TextureTargetSet::RecomputeTargetParameterBlock()
	{
		glm::vec4 targetDimensions(0.f, 0.f, 0.f, 0.f);
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

		RenderTargetParams targetParams;
		targetParams.g_targetResolution = targetDimensions;

		m_targetParameterBlock->Commit(targetParams);
	}


	void TextureTargetSet::RecomputeNumColorTargets()
	{
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


	void TextureTargetSet::RecomputeInternalState()
	{
		if (!m_targetStateDirty)
		{
			return;
		}
		m_targetStateDirty = false;

		RecomputeNumColorTargets();
		RecomputeTargetParameterBlock(); // Must happen after recounting the targets
		ComputeDataHash();
	}


	void TextureTargetSet::ComputeDataHash()
	{
		ResetDataHash();
		
		for (uint8_t slot = 0; slot < m_colorTargets.size(); slot++)
		{
			if (m_colorTargets[slot].HasTexture())
			{
				AddDataBytesToHash(m_colorTargets[slot].GetTexture()->GetTextureParams().m_format);
			}
		}
		if (HasDepthTarget())
		{
			AddDataBytesToHash(m_depthStencilTarget.GetTexture()->GetTextureParams().m_format);
		}		
	}


	uint64_t TextureTargetSet::GetTargetSetSignature()
	{
		RecomputeInternalState();
		return GetDataHash();
	}
}
