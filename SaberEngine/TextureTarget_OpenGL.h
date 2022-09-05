#pragma once

#include "TextureTarget_Platform.h"
#include <GL/glew.h>


namespace opengl
{
	class TextureTarget : public virtual platform::TextureTarget
	{
	public:
	
		struct PlatformParams : public virtual platform::TextureTarget::PlatformParams
		{
			PlatformParams();
			~PlatformParams();
			
			GLuint m_attachmentPoint;	// E.g. GL_COLOR_ATTACHMENT0 + i, GL_DEPTH_ATTACHMENT, etc
			GLuint m_drawBuffer;		// Which of the 4 color buffers should be drawn into for the DEFAULT framebuffer
			GLuint m_readBuffer;		// Which color buffer to use for subsequent reads

			GLuint m_renderBufferObject;	// Handle for non-sampleable targets (eg. depth/stencil)
		};
	};


	class TextureTargetSet : public virtual platform::TextureTargetSet
	{
	public:
		struct PlatformParams : public virtual platform::TextureTargetSet::PlatformParams
		{
			PlatformParams();
			~PlatformParams() override;

			GLuint m_frameBufferObject;			
		};

		// Static members:
		static void CreateColorTargets(gr::TextureTargetSet& targetSet);
		static void AttachColorTargets(gr::TextureTargetSet const& targetSet, uint32_t face, uint32_t mipLevel, bool doBind);

		static void CreateDepthStencilTarget(gr::TextureTargetSet& targetSet);
		static void AttachDepthStencilTarget(gr::TextureTargetSet const& targetSet, bool doBind);

		static uint32_t MaxColorTargets();
	};
}