// © 2022 Adam Badke. All rights reserved.
#include "Context_OpenGL.h"
#include "SwapChain_OpenGL.h"
#include "Texture_OpenGL.h"
#include "TextureTarget_OpenGL.h"

#include "Core/Assert.h"
#include "Core/Config.h"
#include "Core/Logger.h"

#include "Core/Util/CastUtils.h"


namespace
{
	void SetDepthWriteMode(re::TextureTarget const& textureTarget)
	{
		if (textureTarget.GetTargetParams().m_textureView.DepthStencilWritesEnabled())
		{
			glDepthMask(GL_TRUE);
		}
		else
		{
			glDepthMask(GL_FALSE);
		}
	}


	constexpr GLenum GetTextureTargetEnum(re::Texture::Dimension dimension)
	{
		switch (dimension)
		{
		case re::Texture::Dimension::Texture1D: return GL_TEXTURE_1D;
		case re::Texture::Dimension::Texture1DArray: return GL_TEXTURE_1D_ARRAY;
		case re::Texture::Dimension::Texture2D: return GL_TEXTURE_2D;
		case re::Texture::Dimension::Texture2DArray: return GL_TEXTURE_2D_ARRAY;
		case re::Texture::Dimension::Texture3D: return GL_TEXTURE_3D;
		case re::Texture::Dimension::TextureCube: return GL_TEXTURE_CUBE_MAP;
		case re::Texture::Dimension::TextureCubeArray: return GL_TEXTURE_CUBE_MAP_ARRAY;
		default: return GL_INVALID_ENUM; // This should never happen
		}
		SEStaticAssert(re::Texture::Dimension_Count == 7,
			"This function must be updated if the number of Texture dimensions changes");
	}
}

namespace opengl
{
	/***************/
	// TextureTarget
	/***************/

	//


	/************/
	// Target Set
	/************/
	TextureTargetSet::PlatObj::PlatObj() :
		m_frameBufferObject(GL_NONE)
	{
	}


	TextureTargetSet::PlatObj::~PlatObj()
	{
		// platform object are managed via shared_ptr, so we should deallocate OpenGL resources here
		glDeleteFramebuffers(1, &m_frameBufferObject);
		m_frameBufferObject = GL_NONE;
	}


	void TextureTargetSet::CreateColorTargets(re::TextureTargetSet const& targetSet)
	{
		opengl::TextureTargetSet::PlatObj* targetSetParams =
			targetSet.GetPlatformObject()->As<opengl::TextureTargetSet::PlatObj*>();

		// This is a bit of a hack: If we have a color target, we create it. If we have neither a color nor depth
		// target, we assume this is the default framebuffer and create the color target here now well. This might not
		// always be the case (e.g. could be an error, or we might only want to bind color or depth seperately etc), but
		// for now it works
		if (!targetSet.HasColorTarget() && targetSet.HasDepthTarget())
		{
			return;
		}

		SEAssert(targetSetParams->m_isCommitted, "Target set has not been committed");

		SEAssert(util::CheckedCast<uint32_t>(targetSet.GetScissorRect().Left()) >= targetSet.GetViewport().xMin() &&
			util::CheckedCast<uint32_t>(targetSet.GetScissorRect().Top()) >= targetSet.GetViewport().yMin() &&
			util::CheckedCast<uint32_t>(targetSet.GetScissorRect().Right()) <= targetSet.GetViewport().Width() &&
			util::CheckedCast<uint32_t>(targetSet.GetScissorRect().Bottom()) <= targetSet.GetViewport().Height(),
			"Scissor rectangle is out of bounds of the viewport");

		// Configure the framebuffer and each texture target:
		bool foundTarget = false;
		uint32_t targetWidth = 0;
		uint32_t targetHeight = 0;
		std::vector<GLenum> drawBuffers(targetSet.GetColorTargets().size());
		uint32_t numDrawBuffers = 0;
		for (uint32_t i = 0; i < targetSet.GetColorTargets().size(); i++)
		{
			re::TextureTarget const& colorTarget = targetSet.GetColorTarget(i);

			if (!colorTarget.HasTexture())
			{
				break;
			}

			opengl::TextureTarget::PlatObj* targetPlatObj =
				colorTarget.GetPlatformObject()->As<opengl::TextureTarget::PlatObj*>();

			SEAssert(!targetPlatObj->m_isCreated, "Target has already been created");
			targetPlatObj->m_isCreated = true;

			core::InvPtr<re::Texture> const& texture = colorTarget.GetTexture();

			re::Texture::TextureParams const& textureParams = texture->GetTextureParams();
			SEAssert((textureParams.m_usage & re::Texture::Usage::ColorTarget) ||
				(textureParams.m_usage & re::Texture::Usage::SwapchainColorProxy), // Not currently used
				"Attempting to bind a color target with a different texture use parameter");

			// Validate the texture dimensions:
			uint32_t targetMip = re::Texture::k_allMips; // Invalid
			re::TextureView const& targetTexView = colorTarget.GetTargetParams().m_textureView;
			switch (targetTexView.m_viewDimension)
			{
			case re::Texture::Texture1D:
			{
				SEAssert(targetTexView.Texture1D.m_mipLevels == 1, "Target view describes multiple subresources");
				targetMip = targetTexView.Texture1D.m_firstMip;
			}
			break;
			case re::Texture::Texture1DArray:
			{
				SEAssert(targetTexView.Texture1DArray.m_mipLevels == 1 && targetTexView.Texture1DArray.m_arraySize == 1,
					"Target view describes multiple subresources");
				targetMip = targetTexView.Texture1DArray.m_firstMip;
			}
			break;
			case re::Texture::Texture2D:
			{
				SEAssert(targetTexView.Texture2D.m_mipLevels == 1, "Target view describes multiple subresources");
				targetMip = targetTexView.Texture2D.m_firstMip;
			}
			break;
			case re::Texture::Texture2DArray:
			{
				SEAssert(targetTexView.Texture2DArray.m_mipLevels == 1 && targetTexView.Texture2DArray.m_arraySize == 1,
					"Target view describes multiple subresources");
				targetMip = targetTexView.Texture2DArray.m_firstMip;
			}
			break;
			case re::Texture::Texture3D:
			{
				SEAssert(targetTexView.Texture3D.m_mipLevels == 1,
					"Target view describes multiple subresources");
				targetMip = targetTexView.Texture3D.m_firstMip;
			}
			break;
			case re::Texture::TextureCube:
			{
				SEAssert(targetTexView.TextureCube.m_mipLevels == 1,
					"Target view describes multiple subresources");
				targetMip = targetTexView.TextureCube.m_firstMip;
			}
			break;
			case re::Texture::TextureCubeArray:
			{
				SEAssert(targetTexView.TextureCubeArray.m_mipLevels == 1 && 
					targetTexView.TextureCubeArray.m_numCubes == 1,
					"Target view describes multiple subresources");
				targetMip = targetTexView.TextureCubeArray.m_firstMip;
			}
			break;
			default: SEAssertF("Invalid dimension");
			}

			glm::vec4 const& subresourceDimensions = texture->GetMipLevelDimensions(targetMip);
			const uint32_t mipWidth = static_cast<uint32_t>(subresourceDimensions.x);
			const uint32_t mipHeight = static_cast<uint32_t>(subresourceDimensions.y);
			if (!foundTarget)
			{
				foundTarget = true;

				targetWidth = mipWidth;
				targetHeight = mipHeight;
			}
			else
			{
				SEAssert(targetWidth == mipWidth && targetHeight == mipHeight,
					"All framebuffer textures must have the same dimensions");
			}

			// Record the texture in our drawbuffers array:
			// Note: We attach to the same slot/binding index as the texuture has in the target set
			drawBuffers[numDrawBuffers++] = GL_COLOR_ATTACHMENT0 + i;
		}

		// Create framebuffer (not required if this target set represents the default framebuffer):
		if (foundTarget)
		{
			if (!glIsFramebuffer(targetSetParams->m_frameBufferObject))
			{
				glGenFramebuffers(1, &targetSetParams->m_frameBufferObject);
				glBindFramebuffer(GL_FRAMEBUFFER, targetSetParams->m_frameBufferObject);

				// RenderDoc object name:
				glObjectLabel(GL_FRAMEBUFFER, targetSetParams->m_frameBufferObject, -1, targetSet.GetName().c_str());

				SEAssert(glIsFramebuffer(targetSetParams->m_frameBufferObject),
					"Failed to create framebuffer object during texture creation");
			}

			// Attach the textures now that we know the framebuffer is created ("Named" DSA function : no need to
			// explicitely bind the framebuffer first)
			glNamedFramebufferDrawBuffers(targetSetParams->m_frameBufferObject, numDrawBuffers, drawBuffers.data());

			// For now, ensure the viewport dimensions are within the target dimensions
			SEAssert(targetSet.GetViewport().Width() <= targetWidth &&
				targetSet.GetViewport().Height() <= targetHeight,
				"Viewport is larger than the color targets");
		}
		else if (!targetSet.GetDepthStencilTarget().HasTexture())
		{
			LOG_WARNING("Texture target set \"%s\" has no color/depth targets. Assuming it is the default COLOR framebuffer", 
				targetSet.GetName().c_str());
			targetSetParams->m_frameBufferObject = 0;
		}
		else
		{
			SEAssertF("Attempting to bind color targets on a target set that only contains a depth target");
		}
	}


	void TextureTargetSet::AttachColorTargets(re::TextureTargetSet const& targetSet)
	{
		opengl::TextureTargetSet::PlatObj const* targetSetParams =
			targetSet.GetPlatformObject()->As<opengl::TextureTargetSet::PlatObj const*>();

		SEAssert(targetSetParams->m_frameBufferObject == 0 ||
				glIsFramebuffer(targetSetParams->m_frameBufferObject),
			"Cannot bind nonexistant framebuffer");

		glBindFramebuffer(GL_FRAMEBUFFER, targetSetParams->m_frameBufferObject);

		std::vector<GLenum> buffers;
		buffers.reserve(targetSet.GetNumColorTargets());

		core::InvPtr<re::Texture> firstTarget;
		uint32_t firstTargetMipLevel = 0;
		for (uint32_t i = 0; i < targetSet.GetColorTargets().size(); i++)
		{
			if (!targetSet.GetColorTarget(i).HasTexture())
			{
				break;
			}
			
			core::InvPtr<re::Texture> const& texture = targetSet.GetColorTarget(i).GetTexture();
			SEAssert(texture->GetPlatformObject()->m_isCreated, "Texture is not created");

			re::Texture::TextureParams const& textureParams = texture->GetTextureParams();
			opengl::Texture::PlatObj const* texPlatformParams =
				texture->GetPlatformObject()->As<opengl::Texture::PlatObj const*>();

			opengl::TextureTarget::PlatObj const* targetPlatformParams =
				targetSet.GetColorTarget(i).GetPlatformObject()->As<opengl::TextureTarget::PlatObj const*>();

			SEAssert((textureParams.m_usage & re::Texture::Usage::ColorTarget) ||
				(textureParams.m_usage & re::Texture::Usage::SwapchainColorProxy),
				"Attempting to bind a color target with a different texture use parameter");

			re::TextureTarget::TargetParams const& targetParams = targetSet.GetColorTarget(i).GetTargetParams();

			re::TextureView const& texView = targetParams.m_textureView;

			const GLuint textureID = opengl::Texture::GetOrCreateTextureView(texture, texView);

			glNamedFramebufferTexture( // Note: "Named" DSA function: no need to explicitely bind the framebuffer first
				targetSetParams->m_frameBufferObject,	// framebuffer
				GL_COLOR_ATTACHMENT0 + i,				// attachment
				textureID,								// texture
				0);										// level: 0 as it's relative to the texView

			uint32_t firstMip = re::Texture::k_allMips; // Invalid
			switch (texView.m_viewDimension)
			{
			case re::Texture::Texture1D:
			{
				firstMip = texView.Texture1D.m_firstMip;
			}
			break;
			case re::Texture::Texture1DArray:
			{
				firstMip = texView.Texture1DArray.m_firstMip;
			}
			break;
			case re::Texture::Texture2D:
			{
				firstMip = texView.Texture2D.m_firstMip;
			}
			break;
			case re::Texture::Texture2DArray:
			{
				firstMip = texView.Texture2DArray.m_firstMip;
			}
			break;
			case re::Texture::Texture3D:
			{
				firstMip = texView.Texture3D.m_firstMip;
			}
			break;
			case re::Texture::TextureCube:
			case re::Texture::TextureCubeArray:
			{
				SEAssertF("Invalid dimension for a color target");
			}
			break;
			default: SEAssertF("Invalid dimension");
			}

			// Record the attachment point so we can set the draw buffers later on:
			buffers.emplace_back(GL_COLOR_ATTACHMENT0 + i);

			SEAssert(firstTarget == nullptr ||
				(texture->GetMipLevelDimensions(firstMip).x ==
						firstTarget->GetMipLevelDimensions(firstTargetMipLevel).x &&
					texture->GetMipLevelDimensions(firstMip).y ==
						firstTarget->GetMipLevelDimensions(firstTargetMipLevel).y),
				"All framebuffer textures must have the same dimension");

			if (firstTarget == nullptr)
			{
				firstTarget = texture;
				firstTargetMipLevel = firstMip;
			}
		}

		if (!buffers.empty())
		{
			glNamedFramebufferDrawBuffers(
				targetSetParams->m_frameBufferObject,
				static_cast<GLsizei>(buffers.size()),
				buffers.data());

			glm::vec4 const& mipDimensions = firstTarget->GetMipLevelDimensions(firstTargetMipLevel);
			glViewport(
				0,
				0,
				static_cast<GLsizei>(mipDimensions.x),
				static_cast<GLsizei>(mipDimensions.y));

			glScissor(
				0,										// Upper-left corner coordinates: X
				0,										// Upper-left corner coordinates: Y
				static_cast<GLsizei>(mipDimensions.x),	// Width
				static_cast<GLsizei>(mipDimensions.y));	// Height

			// Verify the framebuffer (as we actually had color textures to attach)
			const GLenum result = glCheckNamedFramebufferStatus(targetSetParams->m_frameBufferObject, GL_FRAMEBUFFER);
			SEAssert(result == GL_FRAMEBUFFER_COMPLETE, "Framebuffer is not complete");
		}
	}


	void TextureTargetSet::CreateDepthStencilTarget(re::TextureTargetSet const& targetSet)
	{
		// This is a bit of a hack: If we have a depth target, we create it. If we have neither a color nor depth
		// target, we assume this is the default framebuffer and create the depth target here now well. This might not
		// always be the case (e.g. could be an error, or we might only want to bind color or depth seperately etc), but
		// for now it works
		if (!targetSet.HasDepthTarget() && targetSet.HasColorTarget())
		{
			return;
		}

		opengl::TextureTargetSet::PlatObj* targetSetParams =
			targetSet.GetPlatformObject()->As<opengl::TextureTargetSet::PlatObj*>();

		SEAssert(targetSetParams->m_isCommitted, "Target set has not been committed");

		if (targetSet.GetDepthStencilTarget().HasTexture())
		{
			re::TextureTarget const* depthStencilTarget = &targetSet.GetDepthStencilTarget();
			opengl::TextureTarget::PlatObj* depthTargetPlatObj =
				depthStencilTarget->GetPlatformObject()->As<opengl::TextureTarget::PlatObj*>();
			
			SEAssert(!depthTargetPlatObj->m_isCreated, "Target has already been created");
			depthTargetPlatObj->m_isCreated = true;

			core::InvPtr<re::Texture> const& depthStencilTex = depthStencilTarget->GetTexture();

			// Create framebuffer:
			re::Texture::TextureParams const& depthTextureParams = depthStencilTex->GetTextureParams();
			SEAssert((depthTextureParams.m_usage & re::Texture::Usage::DepthTarget),
				"Attempting to bind a depth target with a different texture use parameter");

			if (!glIsFramebuffer(targetSetParams->m_frameBufferObject))
			{
				glGenFramebuffers(1, &targetSetParams->m_frameBufferObject);
				glBindFramebuffer(GL_FRAMEBUFFER, targetSetParams->m_frameBufferObject);

				// RenderDoc object name:
				glObjectLabel(GL_FRAMEBUFFER, targetSetParams->m_frameBufferObject, -1, targetSet.GetName().c_str());

				SEAssert(glIsFramebuffer(targetSetParams->m_frameBufferObject),
					"Failed to create framebuffer object during texture creation");
			}
			else
			{
				glBindFramebuffer(GL_FRAMEBUFFER, targetSetParams->m_frameBufferObject);
			}

			// For now, ensure the viewport dimensions are within the target dimensions
			SEAssert(targetSet.GetViewport().Width() <= depthStencilTex->Width() &&
				targetSet.GetViewport().Height() <= depthStencilTex->Height(),
				"Viewport is larger than the depth target");
		}
		else if (!targetSet.HasTargets())
		{
			LOG_WARNING("Texture target set \"%s\" has no color/depth targets. Assuming it is the default DEPTH framebuffer",
				targetSet.GetName().c_str());
			targetSetParams->m_frameBufferObject = 0;
		}
		else
		{
			SEAssertF("Attempting to bind depth target on a target set that only contains a color targets");
		}
	}


	void TextureTargetSet::AttachDepthStencilTarget(re::TextureTargetSet const& targetSet)
	{
		if (targetSet.GetDepthStencilTarget().HasTexture())
		{
			re::TextureTarget const& depthTarget = targetSet.GetDepthStencilTarget();

			core::InvPtr<re::Texture> const& depthTex = depthTarget.GetTexture();
			SEAssert(depthTex->GetPlatformObject()->m_isCreated, "Texture is not created");

			SEAssert((depthTex->GetTextureParams().m_usage & re::Texture::Usage::DepthTarget),
				"Attempting to bind a depth target with a different texture use parameter");

			opengl::TextureTargetSet::PlatObj const* targetSetParams =
				targetSet.GetPlatformObject()->As<opengl::TextureTargetSet::PlatObj const*>();

			opengl::TextureTarget::PlatObj const* depthTargetPlatObj =
				depthTarget.GetPlatformObject()->As<opengl::TextureTarget::PlatObj const*>();

			re::TextureView const& texView = depthTarget.GetTargetParams().m_textureView;

			const GLuint textureID = opengl::Texture::GetOrCreateTextureView(depthTex, texView);

			glNamedFramebufferTexture( // Note: "Named" DSA function: no need to explicitely bind the framebuffer first
				targetSetParams->m_frameBufferObject,		// framebuffer
				GL_DEPTH_ATTACHMENT,						// attachment point. TODO: Support GL_STENCIL_ATTACHMENT
				textureID,									// texture
				0);											// level: 0 as it's relative to the texView
			
			// Verify the framebuffer (as we actually had color textures to attach)
			const GLenum result = glCheckNamedFramebufferStatus(targetSetParams->m_frameBufferObject, GL_FRAMEBUFFER);
			SEAssert(result == GL_FRAMEBUFFER_COMPLETE, "Framebuffer is not complete");

			re::Viewport const& viewport = targetSet.GetViewport();
			glViewport(
				viewport.xMin(),
				viewport.yMin(),
				viewport.Width(),
				viewport.Height());

			re::ScissorRect const& scissorRect = targetSet.GetScissorRect();
			glScissor(
				scissorRect.Left(),		// Upper-left corner coordinates: X
				scissorRect.Top(),		// Upper-left corner coordinates: Y
				scissorRect.Right(),	// Width
				scissorRect.Bottom());	// Height

			SetDepthWriteMode(targetSet.GetDepthStencilTarget());
		}
	}


	void TextureTargetSet::ClearColorTargets(
		bool const* colorClearModes,
		glm::vec4 const* colorClearVals,
		uint8_t numColorClears,
		re::TextureTargetSet const& targetSet)
	{
		SEAssert(colorClearModes && colorClearVals && numColorClears > 0,
			"Invalid color clear args");

		opengl::TextureTargetSet::PlatObj const* targetSetParams =
			targetSet.GetPlatformObject()->As<opengl::TextureTargetSet::PlatObj const*>();

		std::vector<re::TextureTarget> const& colorTargets = targetSet.GetColorTargets();
		
		SEAssert(numColorClears > 0 && numColorClears >= colorTargets.size(),
			"Not enough clear values to cover the number of texture targets");

		for (size_t i = 0; i < colorTargets.size(); ++i)
		{
			if (!colorTargets[i].HasTexture())
			{
				break; // Targets must be bound in monotonically-increasing order from slot 0
			}

			if (colorClearModes[i])
			{
				glClearNamedFramebufferfv(
					targetSetParams->m_frameBufferObject,	// framebuffer
					GL_COLOR,								// buffer
					static_cast<GLint>(i),					// drawbuffer
					&colorClearVals[i].r);					// value
			}
		}
	}


	void TextureTargetSet::ClearTargets(
		bool const* colorClearModes,
		glm::vec4 const* colorClearVals,
		uint8_t numColorClears,
		bool depthClearMode,
		float depthClearVal,
		bool stencilClearMode,
		uint8_t stencilClearVal,
		re::TextureTargetSet const& targetSet)
	{
		SEAssert((colorClearModes != nullptr) == (colorClearVals != nullptr) &&
			(colorClearModes != nullptr) == (numColorClears != 0),
			"Invalid color clear args");

		if (colorClearModes)
		{
			ClearColorTargets(colorClearModes, colorClearVals, numColorClears, targetSet);
		}

		if (targetSet.HasDepthTarget() && (depthClearMode || stencilClearMode))
		{
			ClearDepthStencilTarget(depthClearMode, depthClearVal, stencilClearMode, stencilClearVal, targetSet);
		}
	}


	void TextureTargetSet::ClearDepthStencilTarget(
		bool depthClearMode,
		float depthClearVal,
		bool stencilClearMode,
		uint8_t stencilClearVal,
		re::TextureTargetSet const& targetSet)
	{
		SEAssert((depthClearMode || stencilClearMode) &&
			targetSet.HasDepthTarget() &&
			targetSet.GetDepthStencilTarget().HasTexture(),
			"Invalid parameters for depth/stencil clearing");

		opengl::TextureTargetSet::PlatObj const* targetSetPlatObj =
			targetSet.GetPlatformObject()->As<opengl::TextureTargetSet::PlatObj const*>();

		core::InvPtr<re::Texture> const& depthStencilTex = targetSet.GetDepthStencilTarget().GetTexture();
		re::Texture::TextureParams const& texParams = depthStencilTex->GetTextureParams();

		opengl::TextureTarget::PlatObj const* targetPlatObj =
			targetSet.GetDepthStencilTarget().GetPlatformObject()->As<opengl::TextureTarget::PlatObj const*>();

		// Clear depth:
		if (depthClearMode)
		{
			SEAssert(
				((texParams.m_usage & re::Texture::Usage::DepthTarget) != 0) ||
				((texParams.m_usage & re::Texture::Usage::DepthStencilTarget) != 0),
				"Trying to clear depth on a texture not marked for depth usage");

			glClearNamedFramebufferfv(
				targetSetPlatObj->m_frameBufferObject,	// framebuffer
				GL_DEPTH,									// buffer
				0,											// drawbuffer: Must be 0 for GL_DEPTH / GL_STENCIL
				&depthClearVal);							// value
		}

		// Clear stencil:
		if (stencilClearMode)
		{
			SEAssert(
				((texParams.m_usage & re::Texture::Usage::StencilTarget) != 0) ||
				((texParams.m_usage & re::Texture::Usage::DepthStencilTarget) != 0),
				"Trying to clear stencil on a texture not marked for stencil usage");

			const GLint stencilClearValue = stencilClearVal;

			glClearNamedFramebufferiv(
				targetSetPlatObj->m_frameBufferObject,	// framebuffer
				GL_STENCIL,									// buffer
				0,											// drawbuffer: Must be 0 for GL_DEPTH / GL_STENCIL
				&stencilClearValue);
		}

		// TODO: Use glClearNamedFramebufferfi to clear depth and stencil simultaneously
	}


	static void ClearImageTexturesHelper(
		std::vector<re::RWTextureInput> const& rwTexInputs, void const* clearVal, GLenum clearValType)
	{
		SEAssert(clearValType == GL_FLOAT || clearValType == GL_UNSIGNED_BYTE,
			"Unexpected clear value type");

		for (auto const& rwTexInput : rwTexInputs)
		{
			re::Texture::TextureParams const& texParams = rwTexInput.m_texture->GetTextureParams();

			opengl::Texture::PlatObj const* texPlatObj =
				rwTexInput.m_texture->GetPlatformObject()->As<opengl::Texture::PlatObj const*>();

			re::TextureView const& texView = rwTexInput.m_textureView;

			GLint firstLevel = 0;
			GLint numLevels = 0;

			// We'll update these below for special cases:
			GLint xOffset = 0;
			GLint yOffset = 0;
			GLint zOffset = 0;

			GLsizei width = static_cast<GLsizei>(rwTexInput.m_texture->Width());
			GLsizei height = static_cast<GLsizei>(rwTexInput.m_texture->Height());;
			GLsizei depth = 1;

			switch (rwTexInput.m_textureView.m_viewDimension)
			{
			case re::Texture::Dimension::Texture1D:
			{
				firstLevel = texView.Texture1D.m_firstMip;
				numLevels = texView.Texture1D.m_mipLevels;
			}
			break;
			case re::Texture::Dimension::Texture1DArray:
			{
				firstLevel = texView.Texture1DArray.m_firstMip;
				numLevels = texView.Texture1DArray.m_mipLevels;

				yOffset = texView.Texture1DArray.m_firstArraySlice; // 1D arrays: yOffset = first layer to be cleared
				height = texView.Texture1DArray.m_arraySize; // 1D arrays: height = No. of layers to clear
			}
			break;
			case re::Texture::Dimension::Texture2D:
			{
				firstLevel = texView.Texture2D.m_firstMip;
				numLevels = texView.Texture2D.m_mipLevels;
			}
			break;
			case re::Texture::Dimension::Texture2DArray:
			{
				firstLevel = texView.Texture2DArray.m_firstMip;
				numLevels = texView.Texture2DArray.m_mipLevels;

				zOffset = texView.Texture2DArray.m_firstArraySlice; // 2D arrays: zOffset = first layer to be cleared 
				depth = texView.Texture2DArray.m_arraySize;
			}
			break;
			case re::Texture::Dimension::Texture3D:
			{
				SEAssertF("TODO: Test this when this is hit for the 1st time");

				firstLevel = texView.Texture3D.m_firstMip;
				numLevels = texView.Texture3D.m_mipLevels;

				zOffset = texView.Texture3D.m_firstWSlice;
				depth = texView.Texture3D.m_wSize;
			}
			break;
			case re::Texture::Dimension::TextureCube:
			{
				firstLevel = texView.TextureCube.m_firstMip;
				numLevels = texView.TextureCube.m_mipLevels;

				zOffset = 0; // Cube maps: zOffset = cube map face for the corresponding layer
				depth = 6; // Cube maps: depth = No. of faces to clear
			}
			break;
			case re::Texture::Dimension::TextureCubeArray:
			{
				firstLevel = texView.TextureCubeArray.m_firstMip;
				numLevels = texView.TextureCubeArray.m_mipLevels;

				zOffset = texView.TextureCubeArray.m_first2DArrayFace; // Cube arrays: zOffset = 1st layer-face to clear
				depth = texView.TextureCubeArray.m_numCubes; // Cube arrays: depth = No. of layer-faces to clear
			}
			break;
			default: SEAssertF("Invalid dimension");
			}

			for (GLint level = firstLevel; level < numLevels; ++level)
			{
				glClearTexSubImage(
					texPlatObj->m_textureID,
					level,
					xOffset,
					yOffset,
					zOffset,
					width,
					height,
					depth,
					texPlatObj->m_format,
					clearValType,
					clearVal);
			}
		}
	}

	void TextureTargetSet::ClearImageTextures(
		std::vector<re::RWTextureInput> const& rwTexInputs, glm::vec4 const& clearVal)
	{
		ClearImageTexturesHelper(rwTexInputs, &clearVal.x, GL_FLOAT);
	}


	void TextureTargetSet::ClearImageTextures(
		std::vector<re::RWTextureInput> const& rwTexInputs, glm::uvec4 const& clearVal)
	{
		ClearImageTexturesHelper(rwTexInputs, &clearVal.x, GL_UNSIGNED_BYTE);
	}


	void TextureTargetSet::AttachTargetsAsImageTextures(re::TextureTargetSet const& targetSet)
	{
		SEAssert(!targetSet.GetDepthStencilTarget().HasTexture(),
			"It is not possible to attach a depth buffer as a target to a compute shader");

		std::vector<re::TextureTarget> const& texTargets = targetSet.GetColorTargets();
		for (uint32_t slot = 0; slot < texTargets.size(); slot++)
		{
			re::TextureTarget const& texTarget = texTargets[slot];
			if (!texTarget.HasTexture())
			{
				break;;
			}
			
			core::InvPtr<re::Texture> const& texture = texTarget.GetTexture();
			re::TextureTarget::TargetParams const& targetParams = texTarget.GetTargetParams();
			
			constexpr uint32_t k_accessMode = GL_READ_WRITE;

			opengl::Texture::BindAsImageTexture(texture, slot, targetParams.m_textureView, k_accessMode);
		}
	}


	void TextureTargetSet::CopyTexture(core::InvPtr<re::Texture> const& src, core::InvPtr<re::Texture> const& dst)
	{
		opengl::Texture::PlatObj const* srcPlatObj =
			src->GetPlatformObject()->As<opengl::Texture::PlatObj const*>();

		if (!dst.IsValid()) // If no valid destination is provided, we use the backbuffer
		{
			SEAssert(src->Width() == core::Config::GetValue<int>(core::configkeys::k_windowWidthKey) &&
				src->Height() == core::Config::GetValue<int>(core::configkeys::k_windowHeightKey),
				"Can only copy to the backbuffer from textures with identical dimensions");

			re::TextureTargetSet const* backbufferTargetSet = opengl::SwapChain::GetBackBufferTargetSet(
				srcPlatObj->GetContext()->As<opengl::Context*>()->GetSwapChain()).get();
			
			opengl::TextureTargetSet::PlatObj const* backbufferPlatObj =
				backbufferTargetSet->GetPlatformObject()->As<opengl::TextureTargetSet::PlatObj const*>();

			// We're (currently) just have texture handles, so we create a new FBO for the source texture to be read from
			GLuint srcFBO = 0;
			glGenFramebuffers(1, &srcFBO);
			glBindFramebuffer(GL_FRAMEBUFFER, srcFBO);

			// Attach the source texture to the new FBO:
			glNamedFramebufferTexture(
				srcFBO,						// framebuffer
				GL_COLOR_ATTACHMENT0,		// attachment
				srcPlatObj->m_textureID,	// texture
				0);							// level: 0 as it's relative to the texView

			glNamedFramebufferReadBuffer(srcFBO, GL_COLOR_ATTACHMENT0);

			// Construct the appropriate copy mask:
			GLbitfield copyMask = 0;
			if (src->HasUsageBit(re::Texture::Usage::ColorTarget))
			{
				copyMask |= GL_COLOR_BUFFER_BIT;
			}
			if (src->HasUsageBit(re::Texture::Usage::DepthTarget) || 
				src->HasUsageBit(re::Texture::Usage::DepthStencilTarget))
			{
				copyMask |= GL_DEPTH_BUFFER_BIT;
			}
			if (src->HasUsageBit(re::Texture::Usage::StencilTarget) ||
				src->HasUsageBit(re::Texture::Usage::DepthStencilTarget))
			{
				copyMask |= GL_STENCIL_BUFFER_BIT;
			}
			SEAssert(copyMask != 0, "No copy mask bits set");

			// Get the backbuffer to read its dimensions
			std::shared_ptr<re::TextureTargetSet> const& backbufferTargets = 
				opengl::SwapChain::GetBackBufferTargetSet(srcPlatObj->GetContext()->As<opengl::Context*>()->GetSwapChain());

			glBlitNamedFramebuffer(
				srcFBO,										// GLuint readFramebuffer
				backbufferPlatObj->m_frameBufferObject,		// GLuint drawFramebuffer
				0,											// GLint srcX0
				src->Height(),								// GLint srcY0: Note: We *intentionally* flip Y0/Y1 here to invert the result
				src->Width(),								// GLint srcX1
				0,											// GLint srcY1: Note: We *intentionally* flip Y0/Y1 here to invert the result
				0,											// GLint dstX0
				0,											// GLint dstY0
				backbufferTargets->GetViewport().Width(),	// GLint dstX1
				backbufferTargets->GetViewport().Height(),	// GLint dstY1
				copyMask,				// GLbitfield mask: GL_COLOR_BUFFER_BIT/GL_DEPTH_BUFFER_BIT/GL_STENCIL_BUFFER_BIT
				GL_LINEAR									// GLenum filter: Must be GL_NEAREST/GL_LINEAR
			);

			// Cleanup:
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glDeleteFramebuffers(1, &srcFBO);
		}
		else
		{
			opengl::Texture::PlatObj const* dstPlatObj =
				dst->GetPlatformObject()->As<opengl::Texture::PlatObj const*>();

			glCopyImageSubData(
				srcPlatObj->m_textureID,
				GetTextureTargetEnum(src->GetTextureParams().m_dimension),
				0, // srcLevel TODO: Support copying MIPs
				0, // srcX
				0, // srcY
				0, // srcZ
				dstPlatObj->m_textureID,
				GetTextureTargetEnum(dst->GetTextureParams().m_dimension),
				0, // dstLevel TODO: Support copying MIPs
				0, // dstX
				0, // dstY
				0, // dstZ
				src->Width(),	// srcWidth
				src->Height(),	// srcHeight
				src->GetTextureParams().m_arraySize); // srcDepth
		}
	}
}