// © 2022 Adam Badke. All rights reserved.
#include <GL/glew.h>

#include "Core\Util\CastUtils.h"
#include "Assert.h"
#include "TextureTarget.h"
#include "TextureTarget_OpenGL.h"
#include "Texture_OpenGL.h"
#include "Texture_Platform.h"


namespace
{
	void SetBlendMode(re::TextureTarget const& textureTarget, uint32_t drawBufferIndex)
	{
		const re::TextureTarget::TargetParams::BlendModes blendModes = textureTarget.GetBlendMode();
		if (blendModes.m_srcBlendMode == re::TextureTarget::TargetParams::BlendMode::Disabled)
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
			re::TextureTarget::TargetParams::BlendMode const& platformBlendMode,
			GLenum& blendFactor,
			bool isSrc
			)
		{
			switch (platformBlendMode)
			{
			case re::TextureTarget::TargetParams::BlendMode::Zero:
			{
				blendFactor = GL_ZERO;
			}
			break;
			case re::TextureTarget::TargetParams::BlendMode::One:
			{
				blendFactor = GL_ONE;
			}
			break;
			case re::TextureTarget::TargetParams::BlendMode::SrcColor:
			{
				blendFactor = GL_SRC_COLOR;
			}
			break;
			case re::TextureTarget::TargetParams::BlendMode::OneMinusSrcColor:
			{
				blendFactor = GL_ONE_MINUS_SRC_COLOR;
			}
			break;
			case re::TextureTarget::TargetParams::BlendMode::DstColor:
			{
				blendFactor = GL_DST_COLOR;
			}
			break;
			case re::TextureTarget::TargetParams::BlendMode::OneMinusDstColor:
			{
				blendFactor = GL_ONE_MINUS_DST_COLOR;
			}
			case re::TextureTarget::TargetParams::BlendMode::SrcAlpha:
			{
				blendFactor = GL_SRC_ALPHA;
			}
			case re::TextureTarget::TargetParams::BlendMode::OneMinusSrcAlpha:
			{
				blendFactor = GL_ONE_MINUS_SRC_ALPHA;
			}
			case re::TextureTarget::TargetParams::BlendMode::DstAlpha:
			{
				blendFactor = GL_DST_ALPHA;
			}
			case re::TextureTarget::TargetParams::BlendMode::OneMinusDstAlpha:
			{
				blendFactor = GL_ONE_MINUS_DST_ALPHA;
			}
			break;
			default:
			{
				SEAssertF("Invalid blend mode");
			}
			}
		};

		SetGLBlendFactor(blendModes.m_srcBlendMode, sFactor, true);
		SetGLBlendFactor(blendModes.m_dstBlendMode, dFactor, false);

		glBlendFunci(drawBufferIndex, sFactor, dFactor);
	}


	void SetColorWriteMode(re::TextureTarget const& textureTarget, uint32_t drawBufferIndex)
	{
		re::TextureTarget::TargetParams::ChannelWrite const& channelModes = 
			textureTarget.GetTargetParams().m_channelWriteMode;

		glColorMaski(
			drawBufferIndex,
			channelModes.R == re::TextureTarget::TargetParams::ChannelWrite::Enabled ? GL_TRUE : GL_FALSE,
			channelModes.G == re::TextureTarget::TargetParams::ChannelWrite::Enabled ? GL_TRUE : GL_FALSE,
			channelModes.B == re::TextureTarget::TargetParams::ChannelWrite::Enabled ? GL_TRUE : GL_FALSE,
			channelModes.A == re::TextureTarget::TargetParams::ChannelWrite::Enabled ? GL_TRUE : GL_FALSE);
	}


	void SetDepthWriteMode(re::TextureTarget const& textureTarget)
	{
		re::TextureTarget::TargetParams::ChannelWrite::Mode const& depthWriteMode =
			textureTarget.GetTargetParams().m_channelWriteMode.R;

		switch (depthWriteMode)
		{
		case re::TextureTarget::TargetParams::ChannelWrite::Mode::Enabled:
		{
			glDepthMask(GL_TRUE);
		}
		break;
		case re::TextureTarget::TargetParams::ChannelWrite::Mode::Disabled:
		{
			glDepthMask(GL_FALSE);
		}
		break;
		default:
		{
			SEAssertF("Invalid depth write mode");
		}
		}
	}
}

namespace opengl
{
	/***************/
	// TextureTarget
	/***************/
	TextureTarget::PlatformParams::PlatformParams() :
		m_attachmentPoint(GL_NONE),
		m_drawBuffer(GL_NONE),
		m_readBuffer(GL_NONE),
		m_renderBufferObject(0)
	{
	}

	TextureTarget::PlatformParams::~PlatformParams()
	{
		if (m_renderBufferObject > 0)
		{
			glDeleteRenderbuffers(1, &m_renderBufferObject);
			m_renderBufferObject = 0;
		}		

		m_attachmentPoint = GL_NONE;
		m_drawBuffer = GL_NONE;
		m_readBuffer = GL_NONE;
	}


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
		uint32_t insertIdx = 0;
		for (uint32_t i = 0; i < targetSet.GetColorTargets().size(); i++)
		{
			re::TextureTarget const& colorTarget = targetSet.GetColorTarget(i);
			if (colorTarget.HasTexture())
			{
				opengl::TextureTarget::PlatformParams* targetPlatParams =
					colorTarget.GetPlatformParams()->As<opengl::TextureTarget::PlatformParams*>();

				SEAssert(!targetPlatParams->m_isCreated, "Target has already been created");
				targetPlatParams->m_isCreated = true;

				std::shared_ptr<re::Texture> const& texture = colorTarget.GetTexture();

				re::Texture::TextureParams const& textureParams = texture->GetTextureParams();
				SEAssert((textureParams.m_usage & re::Texture::Usage::ColorTarget) ||
					(textureParams.m_usage & re::Texture::Usage::ComputeTarget) ||
					(textureParams.m_usage & re::Texture::Usage::SwapchainColorProxy), // Not currently used
					"Attempting to bind a color target with a different texture use parameter"); 

				// Validate the texture dimensions:
				const uint32_t targetMip = colorTarget.GetTargetParams().m_targetMip;
				glm::vec4 const& subresourceDimensions = texture->GetSubresourceDimensions(targetMip);
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
				targetPlatParams->m_drawBuffer		= GL_COLOR_ATTACHMENT0 + i;
				//targetPlatformParams->m_readBuffer		= GL_COLOR_ATTACHMENT0 + i; // Not needed...

				// Record the texture in our drawbuffers array:
				drawBuffers[insertIdx++] = targetPlatParams->m_attachmentPoint;
			}
		}

		// Create framebuffer (not required if this targetset represents the default framebuffer):
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
			glDrawBuffers((uint32_t)insertIdx, &drawBuffers[0]);

			// For now, ensure the viewport dimensions are within the target dimensions
			SEAssert(targetSet.GetViewport().Width() <= targetWidth  &&
				targetSet.GetViewport().Height() <= targetHeight,
				"Viewport is larger than the color targets");
		}
		else if (!targetSet.GetDepthStencilTarget())
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
		buffers.reserve(targetSet.GetColorTargets().size());

		std::shared_ptr<re::Texture> firstTarget = nullptr;
		uint32_t firstTargetMipLevel = 0;
		for (uint32_t i = 0; i < targetSet.GetColorTargets().size(); i++)
		{
			if (targetSet.GetColorTarget(i).HasTexture())
			{
				std::shared_ptr<re::Texture> texture = targetSet.GetColorTarget(i).GetTexture();
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

				// Attach a texture object to the bound framebuffer:
				if (textureParams.m_dimension == re::Texture::Dimension::TextureCubeMap)
				{
					glFramebufferTexture2D(
						GL_FRAMEBUFFER,
						targetPlatformParams->m_attachmentPoint,
						GL_TEXTURE_CUBE_MAP_POSITIVE_X + targetParams.m_targetFace,
						texPlatformParams->m_textureID,
						targetParams.m_targetMip);
				}
				else
				{
					glNamedFramebufferTexture(
						targetSetParams->m_frameBufferObject,
						targetPlatformParams->m_attachmentPoint,
						texPlatformParams->m_textureID,
						targetParams.m_targetMip);
				}

				// Record the attachment point so we can set the draw buffers later on:
				buffers.emplace_back(targetPlatformParams->m_attachmentPoint);

				SEAssert(firstTarget == nullptr ||
					(texture->Width() == firstTarget->Width() &&
						texture->Height() == firstTarget->Height()),
					"All framebuffer textures must have the same dimension");

				if (firstTarget == nullptr)
				{
					firstTarget = texture;
					firstTargetMipLevel = targetParams.m_targetMip;
				}
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

			SEAssert(firstTarget != nullptr, "First target cannot be null");
			if (firstTarget->GetNumMips() > 1 && firstTargetMipLevel > 0)
			{
				const glm::vec4 mipDimensions = firstTarget->GetSubresourceDimensions(firstTargetMipLevel);
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
			}
			else
			{
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
			}

			// Verify the framebuffer (as we actually had color textures to attach)
			const GLenum result = glCheckNamedFramebufferStatus(targetSetParams->m_frameBufferObject, GL_FRAMEBUFFER);
			SEAssert(result == GL_FRAMEBUFFER_COMPLETE, "Framebuffer is not complete");

			// Clear the targets AFTER setting color write modes
			ClearColorTargets(targetSet);
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

		if (targetSet.GetDepthStencilTarget())
		{
			opengl::TextureTarget::PlatformParams* depthTargetPlatParams =
				targetSet.GetDepthStencilTarget()->GetPlatformParams()->As<opengl::TextureTarget::PlatformParams*>();
			
			SEAssert(!depthTargetPlatParams->m_isCreated, "Target has already been created");
			depthTargetPlatParams->m_isCreated = true;

			std::shared_ptr<re::Texture const> depthStencilTex = targetSet.GetDepthStencilTarget()->GetTexture();

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
			//depthTargetPlatParams->m_readBuffer		 = GL_NONE; // Not needed...

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
		opengl::TextureTargetSet::PlatformParams const* targetSetParams =
			targetSet.GetPlatformParams()->As<opengl::TextureTargetSet::PlatformParams const*>();

		glBindFramebuffer(GL_FRAMEBUFFER, targetSetParams->m_frameBufferObject);

		if (targetSet.GetDepthStencilTarget())
		{
			std::shared_ptr<re::Texture> const& depthStencilTex = targetSet.GetDepthStencilTarget()->GetTexture();
			SEAssert(depthStencilTex->GetPlatformParams()->m_isCreated, "Texture is not created");

			re::Texture::TextureParams const& textureParams = depthStencilTex->GetTextureParams();
			SEAssert((textureParams.m_usage & re::Texture::Usage::DepthTarget),
				"Attempting to bind a depth target with a different texture use parameter");

			opengl::Texture::PlatformParams const* depthPlatformParams =
				depthStencilTex->GetPlatformParams()->As<opengl::Texture::PlatformParams const*>();

			opengl::TextureTarget::PlatformParams const* depthTargetParams =
				targetSet.GetDepthStencilTarget()->GetPlatformParams()->As<opengl::TextureTarget::PlatformParams const*>();

			SEAssert(targetSet.GetDepthStencilTarget()->GetTexture()->GetNumMips() == 1 && 
				targetSet.GetDepthStencilTarget()->GetTargetParams().m_targetMip == 0,
				"It is unexpected that a depth target has mipmaps");

			// Attach a texture object to the bound framebuffer:
			if (textureParams.m_dimension == re::Texture::Dimension::TextureCubeMap)
			{
				// Attach a level of a texture as a logical buffer of a framebuffer object
				glFramebufferTexture(
					GL_FRAMEBUFFER,							// target
					depthTargetParams->m_attachmentPoint,	// attachment
					depthPlatformParams->m_textureID,		// texure
					targetSet.GetDepthStencilTarget()->GetTargetParams().m_targetMip); // level
			}
			else
			{
				// Attach a texture to a framebuffer object:
				glNamedFramebufferTexture(
					targetSetParams->m_frameBufferObject,
					depthTargetParams->m_attachmentPoint,
					depthPlatformParams->m_textureID,
					targetSet.GetDepthStencilTarget()->GetTargetParams().m_targetMip);
			}

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

			SetDepthWriteMode(*targetSet.GetDepthStencilTarget());

			// Clear the targets AFTER setting depth write modes
			ClearDepthStencilTarget(targetSet);
		}
	}


	void TextureTargetSet::ClearColorTargets(re::TextureTargetSet const& targetSet)
	{
		opengl::TextureTargetSet::PlatformParams const* targetSetParams =
			targetSet.GetPlatformParams()->As<opengl::TextureTargetSet::PlatformParams const*>();

		std::vector<re::TextureTarget> const& colorTargets = targetSet.GetColorTargets();
		for (size_t i = 0; i < colorTargets.size(); i++)
		{
			if (colorTargets[i].GetClearMode() == re::TextureTarget::TargetParams::ClearMode::Enabled && 
				colorTargets[i].HasTexture())
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
		if (targetSet.GetDepthStencilTarget()->GetClearMode() == re::TextureTarget::TargetParams::ClearMode::Enabled &&
			targetSet.HasDepthTarget())
		{
			opengl::TextureTargetSet::PlatformParams const* targetSetParams =
				targetSet.GetPlatformParams()->As<opengl::TextureTargetSet::PlatformParams const*>();

			std::shared_ptr<re::Texture> depthStencilTex = targetSet.GetDepthStencilTarget()->GetTexture();

			opengl::TextureTarget::PlatformParams* depthTargetParams =
				targetSet.GetDepthStencilTarget()->GetPlatformParams()->As<opengl::TextureTarget::PlatformParams*>();

			SEAssert(depthTargetParams->m_drawBuffer == 0, "Drawbuffer must be 0 for depth/stencil targets");

			if ((depthStencilTex->GetTextureParams().m_usage & re::Texture::Usage::DepthTarget) != 0 ||
				(depthStencilTex->GetTextureParams().m_usage & re::Texture::Usage::DepthStencilTarget) != 0)
			{
				glClearNamedFramebufferfv(
					targetSetParams->m_frameBufferObject,	// framebuffer
					GL_DEPTH,								// buffer
					depthTargetParams->m_drawBuffer,		// drawbuffer: Must be 0 for GL_DEPTH
					&targetSet.GetDepthStencilTarget()->GetTexture()->GetTextureParams().m_clear.m_depthStencil.m_depth);	// value
			}

			if ((depthStencilTex->GetTextureParams().m_usage & re::Texture::Usage::StencilTarget) != 0 ||
				(depthStencilTex->GetTextureParams().m_usage & re::Texture::Usage::DepthStencilTarget) != 0)
			{
				const GLint stencilClearValue =
					targetSet.GetDepthStencilTarget()->GetTexture()->GetTextureParams().m_clear.m_depthStencil.m_stencil;

				glClearNamedFramebufferiv(
					targetSetParams->m_frameBufferObject,	// framebuffer
					GL_STENCIL,								// buffer
					depthTargetParams->m_drawBuffer,		// drawbuffer: Must be 0 for GL_STENCIL
					&stencilClearValue);
			}
		}
	}


	void TextureTargetSet::AttachTargetsAsImageTextures(re::TextureTargetSet const& targetSet)
	{
		SEAssert(targetSet.GetDepthStencilTarget() == nullptr,
			"It is not possible to attach a depth buffer as a target to a compute shader");

		std::vector<re::TextureTarget> const& texTargets = targetSet.GetColorTargets();
		for (uint32_t slot = 0; slot < texTargets.size(); slot++)
		{
			if (!texTargets[slot].HasTexture())
			{
				continue;
			}

			std::shared_ptr<re::Texture> texture = texTargets[slot].GetTexture();
			re::TextureTarget::TargetParams const& targetParams = texTargets[slot].GetTargetParams();
			
			constexpr uint32_t k_accessMode = GL_READ_WRITE;

			opengl::Texture::BindAsImageTexture(*texture, slot, targetParams.m_targetMip, k_accessMode);
		}

		// TODO: Support compute target clearing
	}
}