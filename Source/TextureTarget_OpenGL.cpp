// � 2022 Adam Badke. All rights reserved.
#include <GL/glew.h>

#include "DebugConfiguration.h"

#include "TextureTarget.h"
#include "TextureTarget_OpenGL.h"
#include "Texture_OpenGL.h"
#include "Texture_Platform.h"

using std::string;


namespace opengl
{
	/*******************************/
	// TextureTarget Platform Params
	/*******************************/
	TextureTarget::PlatformParams::PlatformParams() :
		m_attachmentPoint(GL_NONE),
		m_drawBuffer(GL_NONE),
		m_readBuffer(GL_NONE),
		m_renderBufferObject(0)
	{
	}

	TextureTarget::PlatformParams::~PlatformParams()
	{
		// Platform params are managed via shared_ptr, so we should deallocate OpenGL resources here
		glDeleteRenderbuffers(1, &m_renderBufferObject);
		m_renderBufferObject = 0;

		m_attachmentPoint = GL_NONE;
		m_drawBuffer = GL_NONE;
		m_readBuffer = GL_NONE;
	}


	/****************************/
	// Target Set Platform Params
	/****************************/
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


	/************/
	// Target Set
	/************/

	void TextureTargetSet::CreateColorTargets(re::TextureTargetSet& targetSet)
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

		SEAssert("Targets have already been created", !targetSetParams->m_colorIsCreated);
		targetSetParams->m_colorIsCreated = true;

		// Configure the framebuffer and each texture target:
		uint32_t attachmentPointOffset = 0; // TODO: Attach to the array index, rather than the offset?
		bool foundTarget = false;
		uint32_t width = 0;
		uint32_t height = 0;
		std::vector<GLenum> drawBuffers(targetSet.GetColorTargets().size());
		uint32_t insertIdx = 0;
		for (uint32_t i = 0; i < targetSet.GetColorTargets().size(); i++)
		{
			if (targetSet.GetColorTarget(i))
			{
				// Create/bind the texture:
				std::shared_ptr<re::Texture> const& texture = targetSet.GetColorTarget(i)->GetTexture();

				re::Texture::TextureParams const& textureParams = texture->GetTextureParams();
				SEAssert("Attempting to bind a color target with a different texture use parameter",
					(textureParams.m_usage & re::Texture::Usage::ColorTarget) ||
					(textureParams.m_usage & re::Texture::Usage::SwapchainColorProxy)); // Not currently used

				// Validate the texture:
				if (!foundTarget)
				{
					foundTarget = true;
					width = texture->Width();
					height = texture->Height();		
				}
				else
				{
					SEAssert("All framebuffer textures must have the same dimension",
						width == texture->Width() &&
						height == texture->Height()
					);
				}
				 
				// Configure the target parameters:
				opengl::TextureTarget::PlatformParams* targetParams =
					targetSet.GetColorTarget(i)->GetPlatformParams()->As<opengl::TextureTarget::PlatformParams*>();

				targetParams->m_attachmentPoint = GL_COLOR_ATTACHMENT0 + attachmentPointOffset;
				targetParams->m_drawBuffer		= GL_COLOR_ATTACHMENT0 + attachmentPointOffset;
				//targetPlatformParams->m_readBuffer		= GL_COLOR_ATTACHMENT0 + attachmentPointOffset; // Not needed...

				// Record the texture in our drawbuffers array:
				drawBuffers[insertIdx++] = targetParams->m_attachmentPoint;

				// TODO: Use renderbuffers for depth/stencil stuff

				// Prepare for next iteration:
				attachmentPointOffset++;
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

				SEAssert("Failed to create framebuffer object during texture creation",
					glIsFramebuffer(targetSetParams->m_frameBufferObject));
			}
			else
			{
				glBindFramebuffer(GL_FRAMEBUFFER, targetSetParams->m_frameBufferObject);
			}
			
			// Attach the textures now that we know the framebuffer is created:
			glDrawBuffers((uint32_t)insertIdx, &drawBuffers[0]);

			// For now, ensure the viewport dimensions match the texture target dimensions
			SEAssert("Color textures are different dimension to the viewport",
				width == targetSet.Viewport().Width() &&
				height == targetSet.Viewport().Height());
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

		SEAssert("Cannot bind nonexistant framebuffer",
			(glIsFramebuffer(targetSetParams->m_frameBufferObject) || 
				targetSetParams->m_frameBufferObject == 0));

		glBindFramebuffer(GL_FRAMEBUFFER, targetSetParams->m_frameBufferObject);

		uint32_t attachmentPointOffset = 0;
		std::shared_ptr<re::Texture> firstTarget = nullptr;
		uint32_t firstTargetMipLevel = 0;
		for (uint32_t i = 0; i < targetSet.GetColorTargets().size(); i++)
		{
			if (targetSet.GetColorTarget(i))
			{
				std::shared_ptr<re::Texture> texture = targetSet.GetColorTarget(i)->GetTexture();
				SEAssert("Texture is not created", texture->GetPlatformParams()->m_isCreated);

				re::Texture::TextureParams const& textureParams = texture->GetTextureParams();
				opengl::Texture::PlatformParams const* texPlatformParams =
					texture->GetPlatformParams()->As<opengl::Texture::PlatformParams const*>();
				opengl::TextureTarget::PlatformParams const* targetPlatformParams =
					targetSet.GetColorTarget(i)->GetPlatformParams()->As<opengl::TextureTarget::PlatformParams const*>();

				SEAssert("Attempting to bind a color target with a different texture use parameter", 
					(textureParams.m_usage & re::Texture::Usage::ColorTarget) ||
					(textureParams.m_usage & re::Texture::Usage::SwapchainColorProxy));

				GLenum texTarget = texPlatformParams->m_texTarget;
				re::TextureTarget::TargetParams const& targetParams = targetSet.GetColorTarget(i)->GetTargetParams();
				if (textureParams.m_dimension == re::Texture::Dimension::TextureCubeMap)
				{
					SEAssert("Invalid cubemap face index", targetParams.m_targetFace <= 5);
					texTarget = GL_TEXTURE_CUBE_MAP_POSITIVE_X + targetParams.m_targetFace;
				}

				// Attach a texture object to the bound framebuffer:
				glFramebufferTexture2D(
					GL_FRAMEBUFFER,
					targetPlatformParams->m_attachmentPoint,
					texTarget,
					texPlatformParams->m_textureID,
					targetParams.m_targetSubesource);

				if (firstTarget == nullptr)
				{
					firstTarget = texture;
					firstTargetMipLevel = targetParams.m_targetSubesource;
				}
				else
				{
					SEAssert("All framebuffer textures must have the same dimension",
					texture->Width() == firstTarget->Width() &&
					texture->Height() == firstTarget->Height());
				}

				// Prepare for next iteration:
				attachmentPointOffset++;
			}
		}

		const bool hasTarget = firstTarget != nullptr;
		if (hasTarget && firstTarget->GetNumMips() > 1 && firstTargetMipLevel > 0)
		{
			const glm::vec4 mipDimensions = firstTarget->GetSubresourceDimensions(firstTargetMipLevel);
			glViewport(
				0,
				0,
				static_cast<GLsizei>(mipDimensions.x),
				static_cast<GLsizei>(mipDimensions.y));
		}
		else
		{
			glViewport(
				targetSet.Viewport().xMin(),
				targetSet.Viewport().yMin(),
				targetSet.Viewport().Width(),
				targetSet.Viewport().Height());
		}

		// Verify the framebuffer (if we actually had any color textures to attach)		
		bool result = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
		if (!result && hasTarget)
		{
			const string errorMsg = 
				"Framebuffer is not complete: " + std::to_string(glCheckFramebufferStatus(GL_FRAMEBUFFER));
			SEAssertF(errorMsg);
		}
	}


	void TextureTargetSet::CreateDepthStencilTarget(re::TextureTargetSet& targetSet)
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

		SEAssert("Targets have already been created", !targetSetParams->m_depthIsCreated);
		targetSetParams->m_depthIsCreated = true;

		if (targetSet.GetDepthStencilTarget())
		{
			std::shared_ptr<re::Texture const> depthStencilTex = targetSet.GetDepthStencilTarget()->GetTexture();

			// Create framebuffer:
			re::Texture::TextureParams const& depthTextureParams = depthStencilTex->GetTextureParams();
			SEAssert("Attempting to bind a depth target with a different texture use parameter",
				(depthTextureParams.m_usage & re::Texture::Usage::DepthTarget));

			if (!glIsFramebuffer(targetSetParams->m_frameBufferObject))
			{
				glGenFramebuffers(1, &targetSetParams->m_frameBufferObject);
				glBindFramebuffer(GL_FRAMEBUFFER, targetSetParams->m_frameBufferObject);

				// RenderDoc object name:
				glObjectLabel(GL_FRAMEBUFFER, targetSetParams->m_frameBufferObject, -1, targetSet.GetName().c_str());

				SEAssert("Failed to create framebuffer object during texture creation",
					glIsFramebuffer(targetSetParams->m_frameBufferObject));
			}
			else
			{
				glBindFramebuffer(GL_FRAMEBUFFER, targetSetParams->m_frameBufferObject);
			}

			// Configure the target parameters:
			opengl::TextureTarget::PlatformParams* depthTargetParams =
				targetSet.GetDepthStencilTarget()->GetPlatformParams()->As<opengl::TextureTarget::PlatformParams*>();

			depthTargetParams->m_attachmentPoint = GL_DEPTH_ATTACHMENT;
			depthTargetParams->m_drawBuffer = GL_NONE;
			//depthTargetParams->m_readBuffer		 = GL_NONE; // Not needed...

			// For now, ensure the viewport dimensions match the texture target dimensions
			SEAssert("Depth texture is a different dimension to the viewport", 
				depthStencilTex->Width() == targetSet.Viewport().Width() &&
				depthStencilTex->Height() == targetSet.Viewport().Height());
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
			SEAssert("Texture is not created", depthStencilTex->GetPlatformParams()->m_isCreated);

			re::Texture::TextureParams const& textureParams = depthStencilTex->GetTextureParams();
			SEAssert("Attempting to bind a depth target with a different texture use parameter",
				(textureParams.m_usage & re::Texture::Usage::DepthTarget));

			opengl::Texture::PlatformParams const* depthPlatformParams =
				depthStencilTex->GetPlatformParams()->As<opengl::Texture::PlatformParams const*>();

			opengl::TextureTarget::PlatformParams const* depthTargetParams =
				targetSet.GetDepthStencilTarget()->GetPlatformParams()->As<opengl::TextureTarget::PlatformParams const*>();

			// Attach a texture object to the bound framebuffer:
			if (textureParams.m_dimension == re::Texture::Dimension::TextureCubeMap)
			{
				// Attach a level of a texture as a logical buffer of a framebuffer object
				glFramebufferTexture(
					GL_FRAMEBUFFER,							// target
					depthTargetParams->m_attachmentPoint,	// attachment
					depthPlatformParams->m_textureID,		// texure
					0);
			}
			else
			{
				// Attach a texture to a framebuffer object:
				glFramebufferTexture2D(
					GL_FRAMEBUFFER,
					depthTargetParams->m_attachmentPoint,
					depthPlatformParams->m_texTarget,
					depthPlatformParams->m_textureID,
					0);										// mip level
			}

			// Verify the framebuffer:
			bool result = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
			if (!result)
			{
				SEAssertF("Framebuffer is not complete: " + std::to_string(glCheckFramebufferStatus(GL_FRAMEBUFFER)));
			}

			SEAssert("TODO: Implement support for correctly setting the viewport dimensions for depth textures with "
				"mip maps. See the color target attach function for an example", 
				targetSet.GetDepthStencilTarget()->GetTexture()->GetNumMips() == 1);
			// TODO: We currently just assume the depth buffer is full resolution, but it doesn't have to be. Leaving
			// this assert to save debugging time at a later date...
			glViewport(
				targetSet.Viewport().xMin(),
				targetSet.Viewport().yMin(),
				targetSet.Viewport().Width(),
				targetSet.Viewport().Height());
		}
	}
}