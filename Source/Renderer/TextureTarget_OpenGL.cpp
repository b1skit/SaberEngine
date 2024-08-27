// © 2022 Adam Badke. All rights reserved.
#include "Core/Assert.h"
#include "TextureTarget.h"
#include "TextureTarget_OpenGL.h"
#include "Texture_OpenGL.h"
#include "Texture_Platform.h"

#include "Core/LogManager.h"

#include "Core/Util/CastUtils.h"

#include <GL/glew.h>


namespace
{
	void SetBlendMode(re::TextureTarget const& textureTarget, uint32_t drawBufferIndex)
	{
		const re::TextureTarget::TargetParams::BlendModes blendModes = textureTarget.GetBlendMode();
		if (blendModes.m_srcBlendMode == re::TextureTarget::BlendMode::Disabled)
		{
			SEAssert(blendModes.m_srcBlendMode == blendModes.m_dstBlendMode,
				"Must disable blending for both source and destination");

			glDisable(GL_BLEND);
			return;
		}

		glEnable(GL_BLEND);

		GLenum sFactor = GL_ONE;
		GLenum dFactor = GL_ZERO;

		auto SetGLBlendFactor = [](
			re::TextureTarget::BlendMode const& platformBlendMode,
			GLenum& blendFactor,
			bool isSrc
			)
		{
			switch (platformBlendMode)
			{
			case re::TextureTarget::BlendMode::Zero:
			{
				blendFactor = GL_ZERO;
			}
			break;
			case re::TextureTarget::BlendMode::One:
			{
				blendFactor = GL_ONE;
			}
			break;
			case re::TextureTarget::BlendMode::SrcColor:
			{
				blendFactor = GL_SRC_COLOR;
			}
			break;
			case re::TextureTarget::BlendMode::OneMinusSrcColor:
			{
				blendFactor = GL_ONE_MINUS_SRC_COLOR;
			}
			break;
			case re::TextureTarget::BlendMode::DstColor:
			{
				blendFactor = GL_DST_COLOR;
			}
			break;
			case re::TextureTarget::BlendMode::OneMinusDstColor:
			{
				blendFactor = GL_ONE_MINUS_DST_COLOR;
			}
			break;
			case re::TextureTarget::BlendMode::SrcAlpha:
			{
				blendFactor = GL_SRC_ALPHA;
			}
			break;
			case re::TextureTarget::BlendMode::OneMinusSrcAlpha:
			{
				blendFactor = GL_ONE_MINUS_SRC_ALPHA;
			}
			break;
			case re::TextureTarget::BlendMode::DstAlpha:
			{
				blendFactor = GL_DST_ALPHA;
			}
			break;
			case re::TextureTarget::BlendMode::OneMinusDstAlpha:
			{
				blendFactor = GL_ONE_MINUS_DST_ALPHA;
			}
			break;
			default:
				SEAssertF("Invalid blend mode");
			}
		};

		SetGLBlendFactor(blendModes.m_srcBlendMode, sFactor, true);
		SetGLBlendFactor(blendModes.m_dstBlendMode, dFactor, false);

		glBlendFunci(drawBufferIndex, sFactor, dFactor);
	}


	void SetColorWriteMode(re::TextureTarget const& textureTarget, uint32_t drawBufferIndex)
	{
		glColorMaski(
			drawBufferIndex,
			textureTarget.WritesColor(re::TextureTarget::Channel::R) ? GL_TRUE : GL_FALSE,
			textureTarget.WritesColor(re::TextureTarget::Channel::G) ? GL_TRUE : GL_FALSE,
			textureTarget.WritesColor(re::TextureTarget::Channel::B) ? GL_TRUE : GL_FALSE,
			textureTarget.WritesColor(re::TextureTarget::Channel::A) ? GL_TRUE : GL_FALSE);
	}


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
		m_frameBufferObject(0)
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

			re::Texture const* texture = colorTarget.GetTexture().get();

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

			// Configure the target parameters:
			// Note: We attach to the same slot/binding index as the texuture has in the target set
			targetPlatParams->m_attachmentPoint = GL_COLOR_ATTACHMENT0 + i;
			targetPlatParams->m_drawBuffer = GL_COLOR_ATTACHMENT0 + i;
			//targetPlatformParams->m_readBuffer		= GL_COLOR_ATTACHMENT0 + i; // Not needed...

			// Record the texture in our drawbuffers array:
			drawBuffers[numDrawBuffers++] = targetPlatParams->m_attachmentPoint;
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
			else
			{
				glBindFramebuffer(GL_FRAMEBUFFER, targetSetParams->m_frameBufferObject);
			}
			
			// Attach the textures now that we know the framebuffer is created:
			glDrawBuffers(numDrawBuffers, drawBuffers.data());

			// For now, ensure the viewport dimensions are within the target dimensions
			SEAssert(targetSet.GetViewport().Width() <= targetWidth  &&
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

		re::Texture const* firstTarget = nullptr;
		uint32_t firstTargetMipLevel = 0;
		for (uint32_t i = 0; i < targetSet.GetColorTargets().size(); i++)
		{
			if (!targetSet.GetColorTarget(i).HasTexture())
			{
				break;
			}
			
			re::Texture const* texture = targetSet.GetColorTarget(i).GetTexture().get();
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

			const GLuint textureID = opengl::Texture::GetOrCreateTextureView(*texture, texView);

			glNamedFramebufferTexture( // Note: "Named" DSA function: no need to explicitely bind the framebuffer first
				targetSetParams->m_frameBufferObject,		// framebuffer
				targetPlatformParams->m_attachmentPoint,	// attachment
				textureID,									// texture
				0);											// level: 0 as it's relative to the texView

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
			buffers.emplace_back(targetPlatformParams->m_attachmentPoint);

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

		// Set the blend modes. Note, we set these even if the targets don't contain textures
		for (uint32_t i = 0; i < targetSet.GetColorTargets().size(); i++)
		{
			SetBlendMode(targetSet.GetColorTarget(i), i);
			SetColorWriteMode(targetSet.GetColorTarget(i), i);
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

			re::Texture const* depthStencilTex = depthStencilTarget->GetTexture().get();

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

			// Configure the target parameters:
			depthTargetPlatParams->m_attachmentPoint = GL_DEPTH_ATTACHMENT;
			depthTargetPlatParams->m_drawBuffer = GL_NONE;
			depthTargetPlatParams->m_readBuffer	= GL_NONE;

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

			SEAssert(depthTarget.GetTexture()->GetNumMips() == 1,
				"It is (currently) unexpected that a depth target has mips");

			re::Texture const* depthTex = depthTarget.GetTexture().get();
			SEAssert(depthTex->GetPlatformParams()->m_isCreated, "Texture is not created");

			SEAssert((depthTex->GetTextureParams().m_usage & re::Texture::Usage::DepthTarget),
				"Attempting to bind a depth target with a different texture use parameter");

			opengl::TextureTargetSet::PlatformParams const* targetSetParams =
				targetSet.GetPlatformParams()->As<opengl::TextureTargetSet::PlatformParams const*>();

			opengl::TextureTarget::PlatformParams const* depthTargetPlatParams =
				depthTarget.GetPlatformParams()->As<opengl::TextureTarget::PlatformParams const*>();

			SEAssert(depthTargetPlatParams->m_attachmentPoint == GL_DEPTH_ATTACHMENT,
				"Currently expecting a depth attachment. TODO: Support GL_STENCIL_ATTACHMENT");

			re::TextureView const& texView = depthTarget.GetTargetParams().m_textureView;

			const GLuint textureID = opengl::Texture::GetOrCreateTextureView(*depthTex, texView);

			glNamedFramebufferTexture( // Note: "Named" DSA function: no need to explicitely bind the framebuffer first
				targetSetParams->m_frameBufferObject,		// framebuffer
				depthTargetPlatParams->m_attachmentPoint,	// attachment
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


	void TextureTargetSet::ClearColorTargets(re::TextureTargetSet const& targetSet)
	{
		opengl::TextureTargetSet::PlatformParams const* targetSetParams =
			targetSet.GetPlatformParams()->As<opengl::TextureTargetSet::PlatformParams const*>();

		std::vector<re::TextureTarget> const& colorTargets = targetSet.GetColorTargets();
		for (size_t i = 0; i < colorTargets.size(); i++)
		{
			if (!colorTargets[i].HasTexture())
			{
				break;  // Targets must be bound in monotonically-increasing order from slot 0
			}

			if (colorTargets[i].GetClearMode() == re::TextureTarget::ClearMode::Enabled)
			{
				opengl::TextureTarget::PlatformParams* targetParams =
					colorTargets[i].GetPlatformParams()->As<opengl::TextureTarget::PlatformParams*>();

				glClearNamedFramebufferfv(
					targetSetParams->m_frameBufferObject,	// framebuffer
					GL_COLOR,								// buffer
					static_cast<GLint>(i),					// drawbuffer
					&colorTargets[i].GetTexture()->GetTextureParams().m_clear.m_color.r);	// value
			}
		}
	}


	void TextureTargetSet::ClearDepthStencilTarget(re::TextureTargetSet const& targetSet)
	{
		if (targetSet.GetDepthStencilTarget().GetClearMode() == re::TextureTarget::ClearMode::Enabled &&
			targetSet.HasDepthTarget())
		{
			opengl::TextureTargetSet::PlatformParams const* targetSetPlatParams =
				targetSet.GetPlatformParams()->As<opengl::TextureTargetSet::PlatformParams const*>();

			re::Texture const* depthStencilTex = targetSet.GetDepthStencilTarget().GetTexture().get();
			re::Texture::TextureParams const& texParams = depthStencilTex->GetTextureParams();

			opengl::TextureTarget::PlatformParams const* targetPlatParams =
				targetSet.GetDepthStencilTarget().GetPlatformParams()->As<opengl::TextureTarget::PlatformParams const*>();

			SEAssert(targetPlatParams->m_drawBuffer == 0, "Drawbuffer must be 0 for depth/stencil targets");

			// Clear depth:
			if ((texParams.m_usage & re::Texture::Usage::DepthTarget) != 0 ||
				(texParams.m_usage & re::Texture::Usage::DepthStencilTarget) != 0)
			{
				glClearNamedFramebufferfv(
					targetSetPlatParams->m_frameBufferObject,	// framebuffer
					GL_DEPTH,									// buffer
					targetPlatParams->m_drawBuffer,				// drawbuffer: Must be 0 for GL_DEPTH / GL_STENCIL
					&texParams.m_clear.m_depthStencil.m_depth);	// value
			}

			// Clear stencil:
			if ((texParams.m_usage & re::Texture::Usage::StencilTarget) != 0 ||
				(texParams.m_usage & re::Texture::Usage::DepthStencilTarget) != 0)
			{
				const GLint stencilClearValue = texParams.m_clear.m_depthStencil.m_stencil;

				glClearNamedFramebufferiv(
					targetSetPlatParams->m_frameBufferObject,	// framebuffer
					GL_STENCIL,									// buffer
					targetPlatParams->m_drawBuffer,				// drawbuffer: Must be 0 for GL_DEPTH / GL_STENCIL
					&stencilClearValue);
			}

			// TODO: Use glClearNamedFramebufferfi to clear depth and stencil simultaneously
		}
	}


	void TextureTargetSet::ClearTargets(re::TextureTargetSet const& targetSet)
	{
		ClearColorTargets(targetSet);
		if (targetSet.HasDepthTarget())
		{
			ClearDepthStencilTarget(targetSet);
		}
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
			
			re::Texture const* texture = texTarget.GetTexture().get();
			re::TextureTarget::TargetParams const& targetParams = texTarget.GetTargetParams();
			
			constexpr uint32_t k_accessMode = GL_READ_WRITE;

			opengl::Texture::BindAsImageTexture(*texture, slot, targetParams.m_textureView, k_accessMode);
		}

		// TODO: Support compute target clearing
	}
}