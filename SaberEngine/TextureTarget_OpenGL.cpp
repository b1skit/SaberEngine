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

	void TextureTargetSet::CreateColorTargets(gr::TextureTargetSet& targetSet)
	{
		opengl::TextureTargetSet::PlatformParams* const targetSetParams =
			dynamic_cast<opengl::TextureTargetSet::PlatformParams*>(targetSet.GetPlatformParams());

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
				SEAssert("Attempting to bind a color target with a different texture use parameter",
					textureParams.m_texUse == gr::Texture::TextureUse::ColorTarget);

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

				texture->Create(); // Internally, we ensure we don't double-create textures/platform params
				 
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
		else if (targetSet.DepthStencilTarget().GetTexture() == nullptr)
		{
			LOG("Texture target set has no color/depth targets. Assuming it is the default framebuffer");
			targetSetParams->m_frameBufferObject = 0;
		}
		else
		{
			SEAssertF("Attempting to bind color targets on a target set that only contains a depth target");
		}
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

		SEAssert("Cannot bind nonexistant framebuffer",
			(glIsFramebuffer(targetSetParams->m_frameBufferObject) || 
				targetSetParams->m_frameBufferObject == 0));

		glBindFramebuffer(GL_FRAMEBUFFER, targetSetParams->m_frameBufferObject);

		uint32_t attachmentPointOffset = 0;
		std::shared_ptr<gr::Texture> firstTarget = nullptr;
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

				SEAssert("Attempting to bind a color target with a different texture use parameter", 
					textureParams.m_texUse == gr::Texture::TextureUse::ColorTarget);

				GLenum texTarget = texPlatformParams->m_texTarget;
				if (textureParams.m_texDimension == gr::Texture::TextureDimension::TextureCubeMap)
				{
					SEAssert("Invalid cubemap face index", (face <= 5));
					texTarget = GL_TEXTURE_CUBE_MAP_POSITIVE_X + face;
				}

				// Attach a texture object to the bound framebuffer:
				glFramebufferTexture2D(
					GL_FRAMEBUFFER,
					targetParams->m_attachmentPoint,
					texTarget,
					texPlatformParams->m_textureID,
					mipLevel);

				if (firstTarget == nullptr)
				{
					firstTarget = texture;
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
		if (hasTarget && firstTarget->GetNumMips() > 1 && mipLevel > 0)
		{
			uint32_t mipSize = firstTarget->GetMipDimension(mipLevel);
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

		// Verify the framebuffer (if we actually had any color textures to attach)		
		bool result = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
		if (!result && hasTarget)
		{
			const string errorMsg = 
				"Framebuffer is not complete: " + std::to_string(glCheckFramebufferStatus(GL_FRAMEBUFFER));
			SEAssertF(errorMsg);
		}
	}


	void TextureTargetSet::CreateDepthStencilTarget(gr::TextureTargetSet& targetSet)
	{
		std::shared_ptr<gr::Texture>& depthStencilTex = targetSet.DepthStencilTarget().GetTexture();

		opengl::TextureTargetSet::PlatformParams* const targetSetParams =
			dynamic_cast<opengl::TextureTargetSet::PlatformParams*>(targetSet.GetPlatformParams());

		if (depthStencilTex != nullptr)
		{
			// Create framebuffer:
			gr::Texture::TextureParams const& depthTextureParams = depthStencilTex->GetTextureParams();
			SEAssert("Attempting to bind a depth target with a different texture use parameter",
				depthTextureParams.m_texUse == gr::Texture::TextureUse::DepthTarget);

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
			// TODO: This is duplicated with color targets: Break it out into a helper function?

			depthStencilTex->Create();

			// Configure the target parameters:
			opengl::TextureTarget::PlatformParams* const depthTargetParams =
				dynamic_cast<opengl::TextureTarget::PlatformParams*>(targetSet.DepthStencilTarget().GetPlatformParams());

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
			LOG("Texture target set has no color or depth targets. Assuming it is the default framebuffer");
			targetSetParams->m_frameBufferObject = 0;
		}
		else
		{
			SEAssertF("Attempting to bind depth target on a target set that only contains a color targets");
		}
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

		glBindFramebuffer(GL_FRAMEBUFFER, targetSetParams->m_frameBufferObject);

		std::shared_ptr<gr::Texture> const& depthStencilTex = targetSet.DepthStencilTarget().GetTexture();

		if (depthStencilTex != nullptr)
		{
			gr::Texture::TextureParams const& textureParams = depthStencilTex->GetTextureParams();
			SEAssert("Attempting to bind a depth target with a different texture use parameter",
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
				SEAssertF("Framebuffer is not complete: " + std::to_string(glCheckFramebufferStatus(GL_FRAMEBUFFER)));
			}

			SEAssert("TODO: Implement support for correctly setting the viewport dimensions for depth textures with "
				"mip maps. See the color target attach function for an example", 
				targetSet.DepthStencilTarget().GetTexture()->GetNumMips() == 1);
			// TODO: We currently just assume the depth buffer is full resolution, but it doesn't have to be. Leaving
			// this assert to save debugging time at a later date...
			glViewport(
				targetSet.Viewport().xMin(),
				targetSet.Viewport().yMin(),
				targetSet.Viewport().Width(),
				targetSet.Viewport().Height());
		}
	}


	uint32_t TextureTargetSet::MaxColorTargets()
	{
		GLint maxColorAttachments = 0;
		glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxColorAttachments);
		return maxColorAttachments;
	}
}