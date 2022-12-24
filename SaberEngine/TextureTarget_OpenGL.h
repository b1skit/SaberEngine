#pragma once

#include "TextureTarget.h"
#include "TextureTarget_Platform.h"
#include <GL/glew.h>


namespace opengl
{
	class TextureTarget
	{
	public:
	
		struct PlatformParams final : public virtual re::TextureTarget::PlatformParams
		{
			PlatformParams();
			~PlatformParams();
			
			GLuint m_attachmentPoint;	// E.g. GL_COLOR_ATTACHMENT0 + i, GL_DEPTH_ATTACHMENT, etc
			GLuint m_drawBuffer;		// Which of the 4 color buffers should be drawn into for the DEFAULT framebuffer
			GLuint m_readBuffer;		// Which color buffer to use for subsequent reads

			GLuint m_renderBufferObject;	// Handle for non-sampleable targets (eg. depth/stencil)
		};
	};


	class TextureTargetSet
	{
	public:
		struct PlatformParams final : public virtual re::TextureTargetSet::PlatformParams
		{
			PlatformParams();
			~PlatformParams() override;

			GLuint m_frameBufferObject;			
		};

		// Static members:
		static void CreateColorTargets(re::TextureTargetSet& targetSet);
		static void AttachColorTargets(re::TextureTargetSet& targetSet, uint32_t face, uint32_t mipLevel);

		static void CreateDepthStencilTarget(re::TextureTargetSet& targetSet);
		static void AttachDepthStencilTarget(re::TextureTargetSet& targetSet);

		static uint32_t MaxColorTargets();
	};
}