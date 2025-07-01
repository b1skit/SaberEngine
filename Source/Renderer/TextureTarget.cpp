// © 2022 Adam Badke. All rights reserved.
#include "SysInfo_Platform.h"
#include "TextureTarget.h"
#include "TextureTarget_Platform.h"
#include "RenderManager.h"

#include "Core/Assert.h"
#include "Core/Config.h"

#include "Core/Util/CastUtils.h"

#include "Renderer/Shaders/Common/TargetParams.h"


namespace
{
	bool TextureCanBeSwapped(
		core::InvPtr<re::Texture> const& existing,
		re::TextureView const& existingView, 
		core::InvPtr<re::Texture> const& replacement,
		re::TextureView const& replacementView)
	{
		re::Texture::TextureParams const& existingParams = existing->GetTextureParams();
		re::Texture::TextureParams const& replacementParams = existing->GetTextureParams();

		// The dimensions/no. of mips doesn't really matter, but would probably be a surprise if they changed
		bool result = existing->GetTextureDimenions() == replacement->GetTextureDimenions();
		result &= existing->GetNumMips() == replacement->GetNumMips();

		// The view dimension doesn't technically need to be the same, but would probably be a suprise if it did
		result &= existingView.m_viewDimension == replacementView.m_viewDimension;

		// Ensure the data hash would be the same:
		result &= existingParams.m_format == replacementParams.m_format;
		result &= (memcmp(&existingView.Flags, &replacementView.Flags, sizeof(re::TextureView::ViewFlags)) == 0);

		return result;
	}
}

namespace re
{
	/**************/
	//TextureTarget
	/**************/

	TextureTarget::TextureTarget(core::InvPtr<re::Texture> texture, TargetParams const& targetParams)
		: m_texture(texture)
		, m_targetParams(targetParams)
	{
		platform::TextureTarget::CreatePlatformObject(*this);
	}


	TextureTarget::~TextureTarget()
	{
		m_texture = nullptr;
		m_platObj = nullptr;
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
		platform::TextureTarget::CreatePlatformObject(*this);
		m_targetParams = rhs.m_targetParams;

		return *this;
	}


	void TextureTarget::ReplaceTexture(core::InvPtr<re::Texture> newTex, re::TextureView const& texView)
	{
		SEAssert(TextureCanBeSwapped(newTex, texView, m_texture, m_targetParams.m_textureView),
			"Replacement texture is incompatible with the existing texture");

		m_texture = newTex;
		m_targetParams.m_textureView = texView;
	}


	void TextureTarget::SetTargetParams(TargetParams const& targetParams)
	{
		m_targetParams = targetParams;
	}


	/**********/
	// Viewport
	/**********/
	Viewport::Viewport()
		: m_xMin(0)
		, m_yMin(0)
		, m_width(core::Config::Get()->GetValue<int>(core::configkeys::k_windowWidthKey))
		, m_height(core::Config::Get()->GetValue<int>(core::configkeys::k_windowHeightKey))
	{
	}


	Viewport::Viewport(uint32_t xMin, uint32_t yMin, uint32_t width, uint32_t height)
		: m_xMin(xMin)
		, m_yMin(yMin)
		, m_width(width)
		, m_height(height)
	{
	}

	Viewport::Viewport(core::InvPtr<re::Texture> const& texture)
		: m_xMin(0)
		, m_yMin(0)
		, m_width(texture->Width())
		, m_height(texture->Height())
	{

	}

	/************/
	// ScissorRect
	/************/
	ScissorRect::ScissorRect()
		: m_left(0)
		, m_top(0)
		, m_right(core::Config::Get()->GetValue<int>(core::configkeys::k_windowWidthKey))
		, m_bottom(core::Config::Get()->GetValue<int>(core::configkeys::k_windowHeightKey))
	{
	}


	ScissorRect::ScissorRect(long left, long top, long right, long bottom)
		: m_left(left)
		, m_top(top)
		, m_right(right)
		, m_bottom(bottom)
	{
	}


	ScissorRect::ScissorRect(core::InvPtr<re::Texture> const& texture)
		: m_left(0)
		, m_top(0)
		, m_right(util::CheckedCast<long>(texture->Width()))
		, m_bottom(util::CheckedCast<long>(texture->Height()))
	{
	}


	/******************/
	// TextureTargetSet
	/******************/

	std::shared_ptr<re::TextureTargetSet> TextureTargetSet::Create(std::string const& name)
	{
		std::shared_ptr<re::TextureTargetSet> newTextureTargetSet = nullptr;
		newTextureTargetSet.reset(new re::TextureTargetSet(name));

		gr::RenderManager::Get()->RegisterForCreate(newTextureTargetSet);

		return newTextureTargetSet;
	}


	std::shared_ptr<re::TextureTargetSet> TextureTargetSet::Create(TextureTargetSet const& rhs, std::string const& name)
	{
		std::shared_ptr<re::TextureTargetSet> newTextureTargetSet = nullptr;
		newTextureTargetSet.reset(new re::TextureTargetSet(rhs, name));

		gr::RenderManager::Get()->RegisterForCreate(newTextureTargetSet);

		return newTextureTargetSet;
	}


	std::shared_ptr<re::TextureTargetSet> TextureTargetSet::Create(
		TextureTargetSet const& rhs, re::TextureTarget::TargetParams const& overrideParams, char const* name)
	{
		std::shared_ptr<re::TextureTargetSet> newTextureTargetSet = nullptr;
		newTextureTargetSet.reset(new re::TextureTargetSet(name));

		for (uint8_t slotIdx = 0; slotIdx < rhs.GetNumColorTargets(); slotIdx++)
		{
			newTextureTargetSet->SetColorTarget(slotIdx, rhs.GetColorTarget(slotIdx).GetTexture(), overrideParams);
		}
		if (rhs.HasDepthTarget())
		{
			newTextureTargetSet->SetDepthStencilTarget(rhs.GetDepthStencilTarget().GetTexture(), overrideParams);
		}

		gr::RenderManager::Get()->RegisterForCreate(newTextureTargetSet);

		return newTextureTargetSet;
	}


	TextureTargetSet::TextureTargetSet(std::string const& name)
		: INamedObject(name)
		, m_numColorTargets(0)
	{
		platform::TextureTargetSet::CreatePlatformObject(*this);

		m_colorTargets.resize(platform::SysInfo::GetMaxRenderTargets());
	}


	TextureTargetSet::TextureTargetSet(TextureTargetSet const& rhs, std::string const& newName)
		: INamedObject(newName)
		, m_numColorTargets(rhs.m_numColorTargets)
		, m_viewport(rhs.m_viewport)
		, m_platObj(nullptr) // Targets are copied, but the target set must be created
	{
		platform::TextureTargetSet::CreatePlatformObject(*this);

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
		m_platObj = nullptr;

		m_numColorTargets = 0;
	}


	re::TextureTarget const& TextureTargetSet::GetColorTarget(uint8_t slot) const
	{
		SEAssert(slot < m_colorTargets.size(), "OOB index");
		return m_colorTargets[slot];
	}


	void TextureTargetSet::SetColorTarget(uint8_t slot, re::TextureTarget const& texTarget)
	{
		SEAssert(!m_platObj->m_isCommitted, "Target sets are immutable after they've been committed");
		SEAssert(slot == 0 || m_colorTargets[slot - 1].HasTexture(), 
			"Targets must be set in monotonically-increasing order");
		
		re::TextureView::ValidateView( // _DEBUG only
			texTarget.GetTexture(),
			texTarget.GetTargetParams().m_textureView);

		m_colorTargets[slot] = texTarget;

		RecomputeNumColorTargets();
	}


	void TextureTargetSet::SetColorTarget(uint8_t slot, core::InvPtr<re::Texture> const& texture)
	{
		SetColorTarget(
			slot, 
			texture, 
			TextureTarget::TargetParams{ .m_textureView = re::TextureView::Texture2DView(0, 1) });
	}


	void TextureTargetSet::SetColorTarget(
		uint8_t slot, core::InvPtr<re::Texture> const& texture, TextureTarget::TargetParams const& targetParams)
	{
		SetColorTarget(slot, re::TextureTarget(texture, targetParams));
	}

	re::TextureTarget const& TextureTargetSet::GetDepthStencilTarget() const
	{
		return m_depthStencilTarget;
	}


	void TextureTargetSet::SetDepthStencilTarget(re::TextureTarget const& depthStencilTarget)
	{
		SEAssert(!m_platObj->m_isCommitted, "Target sets are immutable after they've been created");

		SEAssert((depthStencilTarget.GetTexture()->GetTextureParams().m_usage & re::Texture::Usage::DepthTarget) || 
			(depthStencilTarget.GetTexture()->GetTextureParams().m_usage & re::Texture::Usage::StencilTarget) ||
				(depthStencilTarget.GetTexture()->GetTextureParams().m_usage & re::Texture::Usage::DepthStencilTarget),
			"Texture does not have the correct usage flags to be a depth stencil target");

		re::TextureView::ValidateView( // _DEBUG only
			depthStencilTarget.GetTexture(), 
			depthStencilTarget.GetTargetParams().m_textureView);

		m_depthStencilTarget = re::TextureTarget(depthStencilTarget);
	}


	void TextureTargetSet::SetDepthStencilTarget(core::InvPtr<re::Texture> const& depthStencilTargetTex)
	{
		SetDepthStencilTarget(
			depthStencilTargetTex,
			TextureTarget::TargetParams{ .m_textureView = re::TextureView::Texture2DView(0, 1) });
	}


	void TextureTargetSet::SetDepthStencilTarget(
		core::InvPtr<re::Texture> const& depthStencilTargetTex, re::TextureTarget::TargetParams const& targetParams)
	{
		SetDepthStencilTarget(re::TextureTarget(depthStencilTargetTex, targetParams));
	}


	void TextureTargetSet::ReplaceColorTargetTexture(
		uint8_t slot, 
		core::InvPtr<re::Texture>& newTex, 
		re::TextureView const& texView)
	{
		SEAssert(newTex, "Cannot replace a Target's texture with a null texture");
		SEAssert(m_colorTargets[slot].HasTexture(), "Target does not have a texture to replace");

		m_colorTargets[slot].ReplaceTexture(newTex, texView);
	}


	void TextureTargetSet::ReplaceDepthStencilTargetTexture(
		core::InvPtr<re::Texture> newTex, re::TextureView const& texView)
	{
		SEAssert(newTex, "Cannot replace a Target's texture with a null texture");
		SEAssert(m_depthStencilTarget.HasTexture(), "Target does not have a texture to replace");

		m_depthStencilTarget.ReplaceTexture(newTex, texView);
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
		return GetDepthStencilTarget().HasTexture();
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
			core::InvPtr<re::Texture> depthTargetTex = m_depthStencilTarget.GetTexture();
			targetDimensions = depthTargetTex->GetTextureDimenions();
			foundDimensions = true;
		}
		else
		{
			for (uint8_t slot = 0; slot < m_colorTargets.size(); slot++)
			{
				if (m_colorTargets[slot].HasTexture())
				{
					core::InvPtr<re::Texture> colorTargetTex = m_colorTargets[slot].GetTexture();
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
			const uint32_t xRes = (uint32_t)core::Config::Get()->GetValue<int>(core::configkeys::k_windowWidthKey);
			const uint32_t yRes = (uint32_t)core::Config::Get()->GetValue<int>(core::configkeys::k_windowHeightKey);

			targetDimensions.x = (float)xRes;
			targetDimensions.y = (float)yRes;
			targetDimensions.z = 1.0f / xRes;
			targetDimensions.w = 1.0f / yRes;

			foundDimensions = true;
		}

		return targetDimensions;
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
		SEAssert(!m_platObj->m_isCommitted, "Target sets are immutable after they've been committed");

		// Walk through and check each color target:
		m_numColorTargets = 0;
		for (uint8_t slot = 0; slot < m_colorTargets.size(); slot++)
		{
			if (m_colorTargets[slot].HasTexture())
			{
				m_numColorTargets++;
			}
			else
			{
				break; // Targets must be set in monotonically-increasing order, so we can early-out here
			}
		}
	}


	void TextureTargetSet::ValidateConfiguration() const
	{
		// Note: It is valid in some cases (e.g. compute UAV targets) that the target texture dimensions don't match, so
		// we don't (currently) check for that here.
		//
		// Ideally, this validation would be performed at a later point with knowledge of how the targets will actually
		// be used. The below checks will fail in some perfectly valid cases (e.g. compute stages with targets of 
		// different dimensions, or graphics stages with targets that have TextureViews of different sized subresources)

#if defined(_DEBUG)
		for (uint8_t targetIdx = 1; targetIdx < m_numColorTargets; targetIdx++)
		{
			SEAssert(m_colorTargets[targetIdx].GetTexture()->Width() == m_colorTargets[0].GetTexture()->Width() &&
				m_colorTargets[targetIdx].GetTexture()->Height() == m_colorTargets[0].GetTexture()->Height(),
				"Found color targets with mismatching dimensions");
		}

		SEAssert(!HasColorTarget() ||
			!m_depthStencilTarget.HasTexture() ||
			(m_depthStencilTarget.GetTexture()->Width() == m_colorTargets[0].GetTexture()->Width() &&
				m_depthStencilTarget.GetTexture()->Height() == m_colorTargets[0].GetTexture()->Height()),
			"Found depth target with mismatching dimensions");
#endif
	}


	void TextureTargetSet::Commit()
	{
		SEAssert(!m_platObj->m_isCommitted,
			"Target sets are immutable after they've been committed");

		RecomputeNumColorTargets();
		ComputeDataHash();

		ValidateConfiguration(); // _DEBUG only

		// Commit the TargetData Buffer data, if necessary
		if (m_targetParamsBuffer.IsValid())
		{
			m_targetParamsBuffer.GetBuffer()->Commit(GetTargetParamsBufferData());
		}

		m_platObj->m_isCommitted = true;
	}


	void TextureTargetSet::ComputeDataHash()
	{
		// Don't forget to update TextureCanBeSwapped() if this changes

		ResetDataHash();
		
		// Note: We only hash the properties used for pipeline configuration
		for (uint8_t slot = 0; slot < m_colorTargets.size(); slot++)
		{
			if (m_colorTargets[slot].HasTexture())
			{
				AddDataBytesToHash(m_colorTargets[slot].GetTexture()->GetTextureParams().m_format);
				AddDataBytesToHash(m_colorTargets[slot].GetTargetParams().m_textureView.Flags);
			}
		}
		if (HasDepthTarget())
		{
			AddDataBytesToHash(m_depthStencilTarget.GetTexture()->GetTextureParams().m_format);
			AddDataBytesToHash(m_depthStencilTarget.GetTargetParams().m_textureView.Flags);
		}

		SEStaticAssert(sizeof(re::TextureView) == 64u,
			"Texture view size has changed, make sure anything used for pipeline configuration is hashed here");
	}


	uint64_t TextureTargetSet::GetTargetSetSignature()
	{
		Commit();
		return GetDataHash();
	}


	uint64_t TextureTargetSet::GetTargetSetSignature() const
	{
		SEAssert(HasTargets() && m_platObj->m_isCommitted,
			"Trying to get the signature, but the targets haven't been committed");

		return GetDataHash();
	}


	re::BufferInput const& TextureTargetSet::GetCreateTargetParamsBuffer()
	{
		SEAssert(HasTargets(),
			"Trying to get or create the TargetParams buffer, but no targets have been added");

		// Note: Our buffer must be permanent, as we delay committing buffer data

		if (!m_targetParamsBuffer.IsValid())
		{
			m_targetParamsBuffer = re::BufferInput(
				TargetData::s_shaderName,
				re::Buffer::CreateUncommitted<TargetData>(TargetData::s_shaderName, 
					re::Buffer::BufferParams{
						.m_stagingPool = re::Buffer::StagingPool::Permanent,
						.m_memPoolPreference = re::Buffer::UploadHeap,
						.m_accessMask = re::Buffer::GPURead | re::Buffer::CPUWrite,
						.m_usageMask = re::Buffer::Constant,
					}));
		}

		// NOTE: We'll commit the buffer data when the target set is committed

		return m_targetParamsBuffer;
	}


	TargetData TextureTargetSet::GetTargetParamsBufferData() const
	{
		SEAssert(m_targetParamsBuffer.IsValid(),
			"Trying to get target params buffer data but the target params buffer is invalid. This is unexpected");

		core::InvPtr<re::Texture> srcTex;
		if (HasColorTarget())
		{
			srcTex = m_colorTargets[0].GetTexture();
		}
		else
		{
			srcTex = m_depthStencilTarget.GetTexture();
		}

		return TargetData{
				.g_targetDims = glm::vec4(
					srcTex->Width(),
					srcTex->Height(),
					1.f / srcTex->Width(),
					1.f / srcTex->Height()), 
		};
	}
}
