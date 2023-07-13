// © 2022 Adam Badke. All rights reserved.
#include "Config.h"
#include "SysInfo_Platform.h"
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
		, m_targetStateDirty(true)
		, m_numColorTargets(0)
	{
		platform::TextureTargetSet::CreatePlatformParams(*this);
	
		for (size_t i = 0; i < platform::SysInfo::GetMaxRenderTargets(); i++)
		{
			m_colorTargets.emplace_back(nullptr); // Can't use w/unique_ptr as std::vector::resize wants a copy ctor
		}
		m_depthStencilTarget = nullptr;
	}


	TextureTargetSet::TextureTargetSet(TextureTargetSet const& rhs, std::string const& newName)
		: NamedObject(newName)
		, m_targetStateDirty(true)
		, m_numColorTargets(rhs.m_numColorTargets)
		, m_viewport(rhs.m_viewport)
		, m_platformParams(nullptr) // Targets are copied, but the target set must be created
	{
		platform::TextureTargetSet::CreatePlatformParams(*this);

		for (size_t i = 0; i < platform::SysInfo::GetMaxRenderTargets(); i++)
		{
			m_colorTargets.emplace_back(nullptr); // Can't use w/unique_ptr as std::vector::resize wants a copy ctor

			if (rhs.m_colorTargets[i])
			{
				m_colorTargets[i] = std::make_unique<re::TextureTarget>(*rhs.m_colorTargets[i]);
			}
		}
		m_depthStencilTarget = std::make_unique<re::TextureTarget>(*rhs.m_depthStencilTarget);
	}


	TextureTargetSet& TextureTargetSet::operator=(TextureTargetSet const& rhs)
	{
		if (this == &rhs)
		{
			return *this;
		}

		SetName(rhs.GetName());

		m_colorTargets.clear();
		for (size_t i = 0; i < platform::SysInfo::GetMaxRenderTargets(); i++)
		{
			m_colorTargets.emplace_back(nullptr); // Can't use w/unique_ptr as std::vector::resize wants a copy ctor
			if (rhs.m_colorTargets[i])
			{
				m_colorTargets[i] = std::make_unique<re::TextureTarget>(*rhs.m_colorTargets[i]);
			}
		}
		m_depthStencilTarget = std::make_unique<re::TextureTarget>(*rhs.m_depthStencilTarget);

		m_targetStateDirty = rhs.m_targetStateDirty;
		m_numColorTargets = rhs.m_numColorTargets;
		m_viewport = rhs.m_viewport;
		m_platformParams = rhs.m_platformParams;

		return *this;
	}


	TextureTargetSet::~TextureTargetSet()
	{
		m_colorTargets.clear();
		m_depthStencilTarget = nullptr;
		m_platformParams = nullptr;

		m_targetStateDirty = true;
		m_numColorTargets = 0;
	}


	re::TextureTarget const* TextureTargetSet::GetColorTarget(uint8_t slot) const
	{
		SEAssert("OOB index", slot < m_colorTargets.size()); 
		if (m_colorTargets[slot])
		{
			SEAssert("Slot contains a target, but not a texture. This should not be possible", 
				m_colorTargets[slot]->GetTexture());
			return m_colorTargets[slot].get();
		}
		return nullptr;
	}


	void TextureTargetSet::SetColorTarget(uint8_t slot, re::TextureTarget const* texTarget)
	{
		SEAssert("Cannot set a null target", texTarget);
		SEAssert("Target sets are immutable after they've been created", !m_platformParams->m_colorIsCreated);
		m_colorTargets[slot] = std::make_unique<re::TextureTarget>(*texTarget);
		m_targetStateDirty = true;
	}


	void TextureTargetSet::SetColorTarget(
		uint8_t slot, std::shared_ptr<re::Texture> texture, TextureTarget::TargetParams const& targetParams)
	{
		SEAssert("Target sets are immutable after they've been created", !m_platformParams->m_colorIsCreated);
		m_colorTargets[slot] = std::make_unique<re::TextureTarget>(texture, targetParams);
		m_targetStateDirty = true;
	}


	re::TextureTarget const* TextureTargetSet::GetDepthStencilTarget() const
	{
		if (m_depthStencilTarget)
		{
			SEAssert("Depth stencil target exists, but does not contain a texture. This should not be possible",
				m_depthStencilTarget->GetTexture());
			return m_depthStencilTarget.get();
		}
		return nullptr;
	}


	void TextureTargetSet::SetDepthStencilTarget(re::TextureTarget const* depthStencilTarget)
	{
		SEAssert("Cannot set a null target", depthStencilTarget);
		SEAssert("Target sets are immutable after they've been created", !m_platformParams->m_depthIsCreated);
		m_depthStencilTarget = std::make_unique<re::TextureTarget>(*depthStencilTarget);
		m_targetStateDirty = true;
	}


	void TextureTargetSet::SetDepthStencilTarget(
		std::shared_ptr<re::Texture> depthStencilTarget, re::TextureTarget::TargetParams const& targetParams)
	{
		SEAssert("Target sets are immutable after they've been created", !m_platformParams->m_depthIsCreated);
		m_depthStencilTarget = std::make_unique<re::TextureTarget>(depthStencilTarget, targetParams);
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
		return GetDepthStencilTarget() != nullptr;
	}


	uint8_t TextureTargetSet::GetNumColorTargets()
	{
		RecomputeInternalState();
		return m_numColorTargets;
	}


	glm::vec4 TextureTargetSet::GetTargetDimensions() const
	{
		glm::vec4 targetDimensions(0.f);

		bool foundDimensions = false;

		// Find a single target we can get the resolution details from; This assumes all targets are the same dimensions
		if (m_depthStencilTarget)
		{
			std::shared_ptr<re::Texture> depthTarget = m_depthStencilTarget->GetTexture();
			targetDimensions = depthTarget->GetTextureDimenions();
			foundDimensions = true;
		}
		else
		{
			for (uint8_t slot = 0; slot < m_colorTargets.size(); slot++)
			{
				if (m_colorTargets[slot])
				{
					std::shared_ptr<re::Texture> texTarget = m_colorTargets[slot]->GetTexture();
					targetDimensions = texTarget->GetTextureDimenions();
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
			const uint32_t xRes = (uint32_t)Config::Get()->GetValue<int>(en::Config::k_windowXResValueName);
			const uint32_t yRes = (uint32_t)Config::Get()->GetValue<int>(en::Config::k_windowYResValueName);

			targetDimensions.x = (float)xRes;
			targetDimensions.y = (float)yRes;
			targetDimensions.z = 1.0f / xRes;
			targetDimensions.w = 1.0f / yRes;

			foundDimensions = true;
		}

		return targetDimensions;
	}


	void TextureTargetSet::RecomputeNumColorTargets()
	{
		// Walk through and check each color target:
		m_numColorTargets = 0;
		for (uint8_t slot = 0; slot < m_colorTargets.size(); slot++)
		{
			if (m_colorTargets[slot])
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
		ComputeDataHash();
	}


	void TextureTargetSet::ComputeDataHash()
	{
		ResetDataHash();
		
		for (uint8_t slot = 0; slot < m_colorTargets.size(); slot++)
		{
			if (m_colorTargets[slot])
			{
				AddDataBytesToHash(m_colorTargets[slot]->GetTexture()->GetTextureParams().m_format);
			}
		}
		if (HasDepthTarget())
		{
			AddDataBytesToHash(m_depthStencilTarget->GetTexture()->GetTextureParams().m_format);
		}		
	}


	uint64_t TextureTargetSet::GetTargetSetSignature()
	{
		RecomputeInternalState();
		return GetDataHash();
	}


	uint64_t TextureTargetSet::GetTargetSetSignature() const
	{
		SEAssert("Trying to get the signature, but the target state is dirty", !m_targetStateDirty);
		return GetDataHash();
	}
}
