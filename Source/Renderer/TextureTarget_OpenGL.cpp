// © 2022 Adam Badke. All rights reserved.
#include "Context_OpenGL.h"
#include "SwapChain_OpenGL.h"
#include "TextureTarget.h"
#include "TextureTarget_OpenGL.h"
#include "Texture_OpenGL.h"
#include "Texture_Platform.h"

#include "Core/Assert.h"
#include "Core/Config.h"
#include "Core/Logger.h"

#include "Core/Util/CastUtils.h"

#include <GL/glew.h>


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
	TextureTargetSet::PlatformParams::PlatformParams() :
		m_frameBufferObject(GL_NONE)
	{
	}


	TextureTargetSet::PlatformParams::~PlatformParams()
	{
		// Platform params are managed via shared_ptr, so we should deallocate OpenGL resources here
		glDeleteFramebuffers(1, &m_frameBufferObject);
		m_frameBufferObject = GL_NONE;
	}


	void TextureTargetSet::CreateColorTargets(re::TextureTargetSet const& targetSet)
	{
		opengl::TextureTargetSet::PlatformParams* targetSetParams =
			targetSet.GetPlatformParams()->As<opengl::TextureTargetSet::PlatformParams*>();

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

			opengl::TextureTarget::PlatformParams* targetPlatParams =
				colorTarget.GetPlatformParams()->As<opengl::TextureTarget::PlatformParams*>();

			SEAssert(!targetPlatParams->m_isCreated, "Target has already been created");
			targetPlatParams->m_isCreated = true;

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
		opengl::TextureTargetSet::PlatformParams const* targetSetParams =
			targetSet.GetPlatformParams()->As<opengl::TextureTargetSet::PlatformParams const*>();

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
			SEAssert(texture->GetPlatformParams()->m_isCreated, "Texture is not created");

			re::Texture::TextureParams const& textureParams = texture->GetTextureParams();
			opengl::Texture::PlatformParams const* texPlatformParams =
				texture->GetPlatformParams()->As<opengl::Texture::PlatformParams const*>();

			opengl::TextureTarget::PlatformParams const* targetPlatformParams =
				targetSet.GetColorTarget(i).GetPlatformParams()->As<opengl::TextureTarget::PlatformParams const*>();

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

		opengl::TextureTargetSet::PlatformParams* targetSetParams =
			targetSet.GetPlatformParams()->As<opengl::TextureTargetSet::PlatformParams*>();

		SEAssert(targetSetParams->m_isCommitted, "Target set has not been committed");

		if (targetSet.GetDepthStencilTarget().HasTexture())
		{
			re::TextureTarget const* depthStencilTarget = &targetSet.GetDepthStencilTarget();
			opengl::TextureTarget::PlatformParams* depthTargetPlatParams =
				depthStencilTarget->GetPlatformParams()->As<opengl::TextureTarget::PlatformParams*>();
			
			SEAssert(!depthTargetPlatParams->m_isCreated, "Target has already been created");
			depthTargetPlatParams->m_isCreated = true;

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
			SEAssert(depthTex->GetPlatformParams()->m_isCreated, "Texture is not created");

			SEAssert((depthTex->GetTextureParams().m_usage & re::Texture::Usage::DepthTarget),
				"Attempting to bind a depth target with a different texture use parameter");

			opengl::TextureTargetSet::PlatformParams const* targetSetParams =
				targetSet.GetPlatformParams()->As<opengl::TextureTargetSet::PlatformParams const*>();

			opengl::TextureTarget::PlatformParams const* depthTargetPlatParams =
				depthTarget.GetPlatformParams()->As<opengl::TextureTarget::PlatformParams const*>();

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

		opengl::TextureTargetSet::PlatformParams const* targetSetParams =
			targetSet.GetPlatformParams()->As<opengl::TextureTargetSet::PlatformParams const*>();

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

		opengl::TextureTargetSet::PlatformParams const* targetSetPlatParams =
			targetSet.GetPlatformParams()->As<opengl::TextureTargetSet::PlatformParams const*>();

		core::InvPtr<re::Texture> const& depthStencilTex = targetSet.GetDepthStencilTarget().GetTexture();
		re::Texture::TextureParams const& texParams = depthStencilTex->GetTextureParams();

		opengl::TextureTarget::PlatformParams const* targetPlatParams =
			targetSet.GetDepthStencilTarget().GetPlatformParams()->As<opengl::TextureTarget::PlatformParams const*>();

		// Clear depth:
		if (depthClearMode)
		{
			SEAssert(
				((texParams.m_usage & re::Texture::Usage::DepthTarget) != 0) ||
				((texParams.m_usage & re::Texture::Usage::DepthStencilTarget) != 0),
				"Trying to clear depth on a texture not marked for depth usage");

			glClearNamedFramebufferfv(
				targetSetPlatParams->m_frameBufferObject,	// framebuffer
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
				targetSetPlatParams->m_frameBufferObject,	// framebuffer
				GL_STENCIL,									// buffer
				0,											// drawbuffer: Must be 0 for GL_DEPTH / GL_STENCIL
				&stencilClearValue);
		}

		// TODO: Use glClearNamedFramebufferfi to clear depth and stencil simultaneously
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

		// TODO: Support compute target clearing
	}


	void TextureTargetSet::CopyTexture(core::InvPtr<re::Texture> const& src, core::InvPtr<re::Texture> const& dst)
	{
		opengl::Texture::PlatformParams const* srcPlatParams =
			src->GetPlatformParams()->As<opengl::Texture::PlatformParams const*>();

		if (!dst.IsValid()) // If no valid destination is provided, we use the backbuffer
		{
			SEAssert(src->Width() == core::Config::Get()->GetValue<int>(core::configkeys::k_windowWidthKey) &&
				src->Height() == core::Config::Get()->GetValue<int>(core::configkeys::k_windowHeightKey),
				"Can only copy to the backbuffer from textures with identical dimensions");

			re::TextureTargetSet const* backbufferTargetSet = 
				opengl::SwapChain::GetBackBufferTargetSet(re::Context::GetAs<opengl::Context*>()->GetSwapChain()).get();
			
			opengl::TextureTargetSet::PlatformParams const* backbufferPlatParams =
				backbufferTargetSet->GetPlatformParams()->As<opengl::TextureTargetSet::PlatformParams const*>();

			// We're (currently) just have texture handles, so we create a new FBO for the source texture to be read from
			GLuint srcFBO = 0;
			glGenFramebuffers(1, &srcFBO);
			glBindFramebuffer(GL_FRAMEBUFFER, srcFBO);

			// Attach the source texture to the new FBO:
			glNamedFramebufferTexture(
				srcFBO,						// framebuffer
				GL_COLOR_ATTACHMENT0,		// attachment
				srcPlatParams->m_textureID,	// texture
				0);							// level: 0 as it's relative to the texView

			glNamedFramebufferReadBuffer(srcFBO, GL_COLOR_ATTACHMENT0);

			glBlitNamedFramebuffer(
				srcFBO,
				backbufferPlatParams->m_frameBufferObject,
				0,
				0,
				src->Width(),
				src->Height(),
				0,
				0,
				src->Width(), // dstX1: We assume src has the same dimensions as the backbuffer
				src->Height(), // dstY1: We assume src has the same dimensions as the backbuffer
				GL_COLOR_BUFFER_BIT,
				GL_NEAREST // 
			);

			// Cleanup:
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glDeleteFramebuffers(1, &srcFBO);
		}
		else
		{
			opengl::Texture::PlatformParams const* dstPlatParams =
				dst->GetPlatformParams()->As<opengl::Texture::PlatformParams const*>();

			glCopyImageSubData(
				srcPlatParams->m_textureID,
				GetTextureTargetEnum(src->GetTextureParams().m_dimension),
				0, // srcLevel TODO: Support copying MIPs
				0, // srcX
				0, // srcY
				0, // srcZ
				dstPlatParams->m_textureID,
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