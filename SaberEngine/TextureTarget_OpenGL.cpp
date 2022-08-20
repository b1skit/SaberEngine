#include <GL/glew.h>

#include "BuildConfiguration.h"

#include "grTextureTarget.h"
#include "TextureTarget_OpenGL.h"
#include "Texture_OpenGL.h"
#include "Texture_Platform.h"


namespace opengl
{
	/*******************************/
	// TextureTarget Platform Params
	/*******************************/
	TextureTarget::PlatformParams::PlatformParams() :
		m_attachmentPoint(GL_DEPTH_ATTACHMENT), // TODO: This should default to GL_NONE?
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

	// Binds color targets to sequential texture units, starting with firstTextureUnit
	void TextureTargetSet::CreateColorTargets(
		gr::TextureTargetSet& targetSet, 
		uint32_t firstTextureUnit) // TODO: Assert this is valid (should be within whatever ranges I set) AND/OR set this per target!?!?!?!?!?!?!?!
	{
		
		// TODO: Don't think we need firstTextureUnit at all!!!!!! Just need to write a "hidden" create function for
		// textures and call it here, so the texture is valid and can be attached to the framebuffer!!!!!!!!!!!!!!!!!!!
		// -> It's used for sampler?

		opengl::TextureTargetSet::PlatformParams* const targetSetParams =
			dynamic_cast<opengl::TextureTargetSet::PlatformParams*>(targetSet.GetPlatformParams());

		// Create framebuffer:
		if (!glIsFramebuffer(targetSetParams->m_frameBufferObject))
		{
			glGenFramebuffers(1, &targetSetParams->m_frameBufferObject);

			glBindFramebuffer(GL_FRAMEBUFFER, targetSetParams->m_frameBufferObject);

			assert("Failed to create framebuffer object during texture creation" &&
				glIsFramebuffer(targetSetParams->m_frameBufferObject));
		}

		// Configure the framebuffer and each texture target:
		uint32_t attachmentPointOffset = 0; // TODO: Attach to the array index, rather than the offset?
		bool foundTarget = false;
		uint32_t width = 0;
		uint32_t height = 0;
		std::vector<GLenum> drawBuffers(targetSet.ColorTargets().size());
		uint32_t insertIdx = 0;
		for (uint32_t i = 0; i < targetSet.ColorTargets().size(); i++)
		{
			if (targetSet.ColorTarget(i).GetTexture() != nullptr)
			{
				// Create/bind the texture:
				std::shared_ptr<gr::Texture>& texture = targetSet.ColorTarget(i).GetTexture();

				gr::Texture::TextureParams const& textureParams = texture->GetTextureParams();
				assert("Attempting to bind a color target with a different texture use parameter" &&
					textureParams.m_texUse == gr::Texture::TextureUse::ColorTarget);

				// Validate the texture:
				if (!foundTarget)
				{
					foundTarget = true;
					width = texture->Width();
					height = texture->Height();

/*					if (textureParams.m_texDimension != gr::Texture::TextureDimension::TextureCubeMap)
					{
						glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, width);
						glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT, height);
					}	*/				
				}
				else
				{
					assert("All framebuffer textures must have the same dimension" &&
						width == texture->Width() &&
						height == texture->Height()
					);
				}

				

				// We bind the texture to trigger its create/buffer flow
				// TODO: Trigger this directly without actually binding, as it's not required
				texture->Bind(firstTextureUnit + attachmentPointOffset, true);

				// Configure the target parameters:
				opengl::TextureTarget::PlatformParams* const targetParams =
					dynamic_cast<opengl::TextureTarget::PlatformParams*>(targetSet.ColorTarget(i).GetPlatformParams());

				targetParams->m_attachmentPoint = GL_COLOR_ATTACHMENT0 + attachmentPointOffset;
				targetParams->m_drawBuffer		= GL_COLOR_ATTACHMENT0 + attachmentPointOffset;
				//targetParams->m_readBuffer		= GL_COLOR_ATTACHMENT0 + attachmentPointOffset; // Not needed...

				// Record the texture in our drawbuffers array:
				drawBuffers[insertIdx++] = targetParams->m_attachmentPoint;

				// TODO: Use renderbuffers for depth/stencil stuff

				// Prepare for next iteration:
				attachmentPointOffset++;
			}
		}

		assert("Did not find any textures when creating color target" && insertIdx > 0);
		glDrawBuffers((uint32_t)insertIdx, &drawBuffers[0]);

		// For now, ensure the viewport dimensions match the texture target dimensions
		assert("Color textures are different dimension to the viewport" && 
			width == targetSet.Viewport().Width() &&
			height == targetSet.Viewport().Height());
	}


	void TextureTargetSet::AttachColorTargets(
		gr::TextureTargetSet const& targetSet,
		uint32_t face,
		uint32_t mipLevel, 
		bool doBind)
	{
		// Unbinding:
		if (!doBind)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			return;
		}

		// Binding:
		opengl::TextureTargetSet::PlatformParams const* const targetSetParams =
			dynamic_cast<opengl::TextureTargetSet::PlatformParams const*>(targetSet.GetPlatformParams());

		assert("Cannot bind nonexistant framebuffer " &&
			glIsFramebuffer(targetSetParams->m_frameBufferObject));

		glBindFramebuffer(GL_FRAMEBUFFER, targetSetParams->m_frameBufferObject);

		uint32_t attachmentPointOffset = 0;
		bool hasSetViewport = false;
		uint32_t width = 0;
		uint32_t height = 0;
		for (uint32_t i = 0; i < targetSet.ColorTargets().size(); i++)
		{
			if (targetSet.ColorTarget(i).GetTexture() != nullptr)
			{
				// Create/bind the texture:
				std::shared_ptr<gr::Texture> const& texture = targetSet.ColorTarget(i).GetTexture();
				gr::Texture::TextureParams const& textureParams = texture->GetTextureParams();
				opengl::Texture::PlatformParams* const texPlatformParams =
					dynamic_cast<opengl::Texture::PlatformParams*>(texture->GetPlatformParams());
				opengl::TextureTarget::PlatformParams const* const targetParams =
					dynamic_cast<opengl::TextureTarget::PlatformParams const*>(targetSet.ColorTarget(i).GetPlatformParams());

				assert("Attempting to bind a color target with a different texture use parameter" &&
					textureParams.m_texUse == gr::Texture::TextureUse::ColorTarget);

				GLenum texTarget = texPlatformParams->m_texTarget;
				if (textureParams.m_texDimension == gr::Texture::TextureDimension::TextureCubeMap)
				{
					assert("Invalid cubemap face index" && (face <= 5));
					texTarget = GL_TEXTURE_CUBE_MAP_POSITIVE_X + face;
				}

				// Attach a texture object to the bound framebuffer:
				glFramebufferTexture2D(
					GL_FRAMEBUFFER,
					targetParams->m_attachmentPoint,
					texTarget,
					texPlatformParams->m_textureID,
					mipLevel);

				
				if (!hasSetViewport)
				{
					width = texture->Width();
					height = texture->Height();

					if (texture->GetNumMips() > 1 && mipLevel > 0)
					{
						uint32_t mipSize = texture->GetMipDimension(mipLevel);
						glViewport(
							0,
							0,
							mipSize,
							mipSize);
					}
					else
					{
						glViewport(
							targetSet.Viewport().xMin(),
							targetSet.Viewport().yMin(),
							targetSet.Viewport().Width(),
							targetSet.Viewport().Height());
					}					

					hasSetViewport = true;

					// TODO: MAKE SETTING THE VIEWPORT A PLATFORM-SPECIFIC HELPER FUNCTION
					// -> AVOID DOING IT TWICE WHEN BINDING BOTH COLOR AND DEPTH!!!!!!!!!!!!!!!!!!!!!!!
				}
				else
				{
					assert("All framebuffer textures must have the same dimension" &&
						width == texture->Width() &&
						height == texture->Height());
				}

				// Prepare for next iteration:
				attachmentPointOffset++;
			}
		}

		// Verify the framebuffer:
		assert("Framebuffer has no color targets" && hasSetViewport);
		bool result = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
		if (!result)
		{
			SaberEngine::LOG_ERROR("Framebuffer is not complete: " +
				to_string(glCheckFramebufferStatus(GL_FRAMEBUFFER)));

			assert("Framebuffer is not complete" && result);
		}
	}


	void TextureTargetSet::CreateDepthStencilTarget(gr::TextureTargetSet& targetSet, uint32_t textureUnit)
	{
		std::shared_ptr<gr::Texture>& depthStencilTex = targetSet.DepthStencilTarget().GetTexture();
		assert("Cannot bind null depth target" && depthStencilTex != nullptr);

		// Create framebuffer:
		opengl::TextureTargetSet::PlatformParams* const targetSetParams =
			dynamic_cast<opengl::TextureTargetSet::PlatformParams*>(targetSet.GetPlatformParams());

		gr::Texture::TextureParams const& depthTextureParams = depthStencilTex->GetTextureParams();
		assert("Attempting to bind a depth target with a different texture use parameter" &&
			depthTextureParams.m_texUse == gr::Texture::TextureUse::DepthTarget);

		if (!glIsFramebuffer(targetSetParams->m_frameBufferObject))
		{
			glGenFramebuffers(1, &targetSetParams->m_frameBufferObject);

			glBindFramebuffer(GL_FRAMEBUFFER, targetSetParams->m_frameBufferObject);

			assert("Failed to create framebuffer object during texture creation" &&
				glIsFramebuffer(targetSetParams->m_frameBufferObject));


			//// Specifies the assumed with for a framebuffer object with no attachments
			//// TODO: Is this needed??????????????????????
			//if (depthTextureParams.m_texDimension != gr::Texture::TextureDimension::TextureCubeMap)
			//{
			//	glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, depthStencilTex->Width());
			//	glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT, depthStencilTex->Height());
			//}
			
		}
		// TODO: This is duplicated with color targets: Break it out into a helper function?

		

		// We bind the texture to trigger its create/buffer flow
		depthStencilTex->Bind(textureUnit, true);
		// TODO: Trigger this directly without actually binding, as it's not required


		// Configure the target parameters:
		opengl::TextureTarget::PlatformParams* const depthTargetParams =
			dynamic_cast<opengl::TextureTarget::PlatformParams*>(targetSet.DepthStencilTarget().GetPlatformParams());

		depthTargetParams->m_attachmentPoint = GL_DEPTH_ATTACHMENT;
		depthTargetParams->m_drawBuffer		 = GL_NONE;
		//depthTargetParams->m_readBuffer		 = GL_NONE; // Not needed...

		// For now, ensure the viewport dimensions match the texture target dimensions
		assert("Depth texture is a different dimension to the viewport" &&
			depthStencilTex->Width() == targetSet.Viewport().Width() &&
			depthStencilTex->Height() == targetSet.Viewport().Height());
	}


	void TextureTargetSet::AttachDepthStencilTarget(gr::TextureTargetSet const& targetSet, bool doBind)
	{
		// Unbinding:
		if (!doBind)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			return;
		}

		// Binding:
		opengl::TextureTargetSet::PlatformParams const* const targetSetParams =
			dynamic_cast<opengl::TextureTargetSet::PlatformParams const*>(targetSet.GetPlatformParams());

		assert("Cannot bind nonexistant framebuffer " &&
			glIsFramebuffer(targetSetParams->m_frameBufferObject));
		glBindFramebuffer(GL_FRAMEBUFFER, targetSetParams->m_frameBufferObject);

		std::shared_ptr<gr::Texture> const& depthStencilTex = targetSet.DepthStencilTarget().GetTexture();
		assert("Cannot bind null depth stencil texture" && depthStencilTex != nullptr);

		gr::Texture::TextureParams const& textureParams = depthStencilTex->GetTextureParams();
		assert("Attempting to bind a depth target with a different texture use parameter" &&
			textureParams.m_texUse == gr::Texture::TextureUse::DepthTarget);

		opengl::Texture::PlatformParams* const depthPlatformParams =
			dynamic_cast<opengl::Texture::PlatformParams*>(depthStencilTex->GetPlatformParams());

		opengl::TextureTarget::PlatformParams const* const depthTargetParams =
			dynamic_cast<opengl::TextureTarget::PlatformParams const*>(targetSet.DepthStencilTarget().GetPlatformParams());

		// Attach a texture object to the bound framebuffer:
		if (textureParams.m_texDimension == gr::Texture::TextureDimension::TextureCubeMap)
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
			SaberEngine::LOG_ERROR("Framebuffer is not complete: " +
				to_string(glCheckFramebufferStatus(GL_FRAMEBUFFER)));

			assert("Framebuffer is not complete" && result);
		}

		glViewport(
			targetSet.Viewport().xMin(),
			targetSet.Viewport().yMin(),
			targetSet.Viewport().Width(),
			targetSet.Viewport().Height());

		// TODO: MAKE SETTING THE VIEWPORT A PLATFORM-SPECIFIC HELPER FUNCTION
		// -> AVOID DOING IT TWICE WHEN BINDING BOTH COLOR AND DEPTH!!!!!!!!!!!!!!!!!!!!!!!
	}


	uint32_t TextureTargetSet::MaxColorTargets()
	{
		GLint maxColorAttachments = 0;
		glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxColorAttachments);
		return maxColorAttachments;
	}
}