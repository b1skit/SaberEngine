#include "RenderTexture.h"
#include "CoreEngine.h"
#include "BuildConfiguration.h"
#include "Material.h"


namespace SaberEngine
{
	RenderTexture::RenderTexture() 
		: RenderTexture
		(
			CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("defaultShadowMapWidth"), 
			CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("defaultShadowMapHeight"),
			DEFAULT_RENDERTEXTURE_NAME
		)
	{}	// Do nothing else


	RenderTexture::RenderTexture(int width, int height, string name /*= DEFAULT_RENDERTEXTURE_NAME*/)
	{
		m_width					= width;
		m_height				= height;
		m_numTexels					= width * height;

		m_texturePath			= name;

		m_texels				= nullptr;
		m_resolutionHasChanged	= true;

		// Override default values:
		//-------------------------
		m_internalFormat		= GL_DEPTH_COMPONENT32F;
		m_format				= GL_DEPTH_COMPONENT;
		type					= GL_FLOAT; // Same as Texture...
		
		m_textureWrapS			= GL_CLAMP_TO_EDGE; // NOTE: Mandatory for non-power-of-two textures
		m_textureWrapT			= GL_CLAMP_TO_EDGE;


		m_textureMinFilter		= GL_LINEAR;
		m_textureMaxFilter		= GL_LINEAR;
	}


	RenderTexture::RenderTexture(RenderTexture const& rhs) : Texture(rhs)
	{
		m_frameBufferObject = 0;	// NOTE: We set the frame buffer to 0, since we don't want to stomp any existing ones

		m_attachmentPoint	= rhs.m_attachmentPoint;

		m_drawBuffer		= rhs.m_drawBuffer;
		m_readBuffer		= rhs.m_readBuffer;
	}


	RenderTexture& RenderTexture::operator=(RenderTexture const& rhs)
	{
		if (this == &rhs)
		{
			return *this;
		}

		Texture::operator=(rhs);

		//frameBufferObject = 0;	// NOTE: We set the frame buffer to 0, since we don't want to stomp any existing ones
		m_frameBufferObject	= rhs.m_frameBufferObject;

		m_attachmentPoint	= rhs.m_attachmentPoint;

		m_drawBuffer = rhs.m_drawBuffer;
		m_readBuffer = rhs.m_readBuffer;

		return *this;
	}


	// NOTE: additionalRTs must be cleaned up by the caller
	// NOTE: The correct attachment points must already be configured for each RenderTexture
	// NOTE: The additionalRT's must have already successfully called Texture::Buffer()
	void RenderTexture::AttachAdditionalRenderTexturesToFramebuffer(RenderTexture** additionalRTs, int numRTs, bool isDepth /*=false*/)
	{
		if (isDepth && numRTs != 1)
		{
			LOG_ERROR("Cannot add more than 1 depth buffer to a framebuffer. Returning without attaching");
			return;
		}

		BindFramebuffer(true);

		if (isDepth)
		{
			(*additionalRTs)->AttachToFramebuffer((*additionalRTs)->TextureTarget());
		}
		else
		{
			for (int currentRT = 0; currentRT < numRTs; currentRT++)
			{
				additionalRTs[currentRT]->AttachToFramebuffer(additionalRTs[currentRT]->TextureTarget());
			}

			// Assemble a list of attachment points
			int totalRTs		= numRTs + 1;
			GLenum* drawBuffers = new GLenum[totalRTs];
			drawBuffers[0]		= m_attachmentPoint;
			for (int i = 1; i < totalRTs; i++)
			{
				drawBuffers[i] = additionalRTs[i - 1]->m_attachmentPoint;
			}

			glDrawBuffers(totalRTs, drawBuffers);
		}		

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		{
			LOG_ERROR("Attaching additional buffers failed")
		}

		// Cleanup:
		BindFramebuffer(false);
	}


	void RenderTexture::Destroy()
	{
		Texture::Destroy();

		glDeleteFramebuffers(1, &m_frameBufferObject);
	}


	bool RenderTexture::Buffer(int textureUnit)
	{
		if (Texture::Buffer(textureUnit)) // Makes required calls to glTexParameteri, binds textureID etc 
		{
			BindFramebuffer(true);

			if (!glIsFramebuffer(m_frameBufferObject))
			{
				glGenFramebuffers(1, &m_frameBufferObject);
				BindFramebuffer(true);
				if (!glIsFramebuffer(m_frameBufferObject))
				{
					LOG_ERROR("Failed to create framebuffer object");
					return false;
				}

				glDrawBuffer(m_drawBuffer); // Sets the color buffer to draw too (eg. GL_NONE for a depth map)
				glReadBuffer(m_readBuffer);

				// Configure framebuffer parameters:
				glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, m_width);
				glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT, m_height);

				// Attach our texture to the framebuffer as a render buffer:
				AttachToFramebuffer(m_texTarget);
			}

			#if defined(DEBUG_SCENEMANAGER_TEXTURE_LOGGING)
				LOG("Render texture setup complete!");
			#endif

			bool result = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
			if (!result)
			{
				LOG_ERROR("Framebuffer is not complete: " + to_string(glCheckFramebufferStatus(GL_FRAMEBUFFER)));
			}

			// Cleanup:
			BindFramebuffer(false);
			glBindTexture(m_texTarget, 0);

			return result;
		}
		else
		{
			LOG_ERROR("Texture buffer failed. Render texture could not be buffered either");

			return false;
		}		
	}
	

	bool RenderTexture::BufferCubeMap(RenderTexture** cubeFaceRTs, int textureUnit)
	{
		// NOTE: This function uses the paramters of cubeFaceRTs[0]

		if (!Texture::BufferCubeMap((Texture**)cubeFaceRTs, textureUnit))
		{
			return false;
		}

		// RenderTexture specific setup:
		LOG("Configuring cube map as RenderTexture: \"" + cubeFaceRTs[0]->TexturePath() + "\"");

		// Generate faces:
		for (int i = 0; i < CUBE_MAP_NUM_FACES; i++)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, cubeFaceRTs[0]->m_internalFormat, cubeFaceRTs[0]->m_width, cubeFaceRTs[0]->m_height, 0, cubeFaceRTs[0]->m_format, cubeFaceRTs[0]->type, NULL);
		}

		// Ensure all of the other textures have the same ID, sampler, and FBO info:
		for (int i = 1; i < CUBE_MAP_NUM_FACES; i++)
		{
			cubeFaceRTs[i]->m_textureID			= cubeFaceRTs[0]->m_textureID;
			cubeFaceRTs[i]->m_samplerID			= cubeFaceRTs[0]->m_samplerID;
			cubeFaceRTs[i]->m_frameBufferObject	= cubeFaceRTs[0]->m_frameBufferObject;
		}

		// Bind framebuffer:
		bool result = true;
		cubeFaceRTs[0]->BindFramebuffer(true);
		if (!glIsFramebuffer(cubeFaceRTs[0]->m_frameBufferObject))
		{
			glGenFramebuffers(1, &cubeFaceRTs[0]->m_frameBufferObject);
			cubeFaceRTs[0]->BindFramebuffer(true);
			if (!glIsFramebuffer(cubeFaceRTs[0]->m_frameBufferObject))
			{
				LOG_ERROR("Failed to create framebuffer object");
				return false;
			}

			// Attach framebuffer as a cube map render buffer:
			glFramebufferTexture(GL_FRAMEBUFFER, cubeFaceRTs[0]->m_attachmentPoint, cubeFaceRTs[0]->m_textureID, 0);		

			glDrawBuffer(cubeFaceRTs[0]->m_drawBuffer);
			glReadBuffer(cubeFaceRTs[0]->m_readBuffer);

			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			{
				LOG_ERROR("Cube map framebuffer is not complete!");
				result = false;
			}
		}

		// Cleanup:
		glBindTexture(cubeFaceRTs[0]->m_texTarget, 0); // Was still bound from Texture::BufferCubeMap()
		cubeFaceRTs[0]->BindFramebuffer(false);

		return result;
	}


	RenderTexture** RenderTexture::CreateCubeMap(int xRes, int yRes, string name /*="UNNAMMED"*/)
	{
		RenderTexture** cubeFaces = new RenderTexture*[CUBE_MAP_NUM_FACES];

		// Attach a texture to each slot:
		for (int i = 0; i < CUBE_MAP_NUM_FACES; i++)
		{
			RenderTexture* cubeRenderTexture = new RenderTexture
			(
				xRes,
				yRes,
				name + "_CubeMap"
			);

			// Configure the texture:
			cubeRenderTexture->TextureTarget()		= GL_TEXTURE_CUBE_MAP;

			cubeRenderTexture->TextureWrap_S()		= GL_CLAMP_TO_EDGE;
			cubeRenderTexture->TextureWrap_T()		= GL_CLAMP_TO_EDGE;
			cubeRenderTexture->TextureWrap_R()		= GL_CLAMP_TO_EDGE;

			cubeRenderTexture->TextureMinFilter()	= GL_NEAREST;
			cubeRenderTexture->TextureMaxFilter()	= GL_NEAREST;

			cubeRenderTexture->InternalFormat()		= GL_DEPTH_COMPONENT32F;
			cubeRenderTexture->Format()				= GL_DEPTH_COMPONENT;
			cubeRenderTexture->Type()				= GL_FLOAT;

			cubeRenderTexture->AttachmentPoint()	= GL_DEPTH_ATTACHMENT;	// Preparing a shadow map by default...
			cubeRenderTexture->DrawBuffer()			= GL_NONE;
			cubeRenderTexture->ReadBuffer()			= GL_NONE;

			// Cache off the texture for buffering when we're done
			cubeFaces[i] = cubeRenderTexture;
		}

		return cubeFaces;
	}


	void RenderTexture::BindFramebuffer(bool doBind)
	{
		if (doBind)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, m_frameBufferObject);
		}
		else
		{
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}
	}


	void RenderTexture::AttachToFramebuffer(GLenum textureTarget, int mipLevel /*= 0*/)
	{
		glFramebufferTexture2D(GL_FRAMEBUFFER, m_attachmentPoint, textureTarget, m_textureID, mipLevel);
	}


	void RenderTexture::CreateRenderbuffer(bool leaveBound /*= true*/, int xRes /*= -1*/, int yRes /*= -1*/)
	{
		if (xRes <= 0 || yRes <= 0)
		{
			xRes = m_width;
			yRes = m_height;
		}

		if (glIsRenderbuffer(m_frameBufferObject) == GL_FALSE)
		{
			glGenRenderbuffers(1, &m_frameBufferObject);
		}

		BindRenderbuffer(true);

		// Allocate storage: 
		// NOTE: For now, we hard code internalFormat == GL_DEPTH_COMPONENT24, as it's all we ever use...
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, xRes, yRes);

		if (leaveBound == false)
		{
			BindRenderbuffer(false);
		}
	}


	void RenderTexture::BindRenderbuffer(bool doBind)
	{
		if (doBind)
		{
			glBindRenderbuffer(GL_RENDERBUFFER, m_frameBufferObject);
		}
		else
		{
			glBindRenderbuffer(GL_RENDERBUFFER, 0);
		}
	}


	void RenderTexture::DeleteRenderbuffer(bool unbind /*= true*/)
	{
		if (unbind == true)
		{
			BindRenderbuffer(false);
		}		

		glDeleteRenderbuffers(1, &m_frameBufferObject);
	}
}


