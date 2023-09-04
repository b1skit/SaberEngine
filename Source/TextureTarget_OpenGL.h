// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "TextureTarget.h"
#include "TextureTarget_Platform.h"
#include <GL/glew.h>


namespace opengl
{
	class TextureTarget
	{
	public:
		struct PlatformParams final : public re::TextureTarget::PlatformParams
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
		struct PlatformParams final : public re::TextureTargetSet::PlatformParams
		{
			PlatformParams();
			~PlatformParams() override;

			GLuint m_frameBufferObject;			
		};

		// Static members:
		static void CreateColorTargets(re::TextureTargetSet const&);
		static void AttachColorTargets(re::TextureTargetSet const&);

		static void CreateDepthStencilTarget(re::TextureTargetSet const&);
		static void AttachDepthStencilTarget(re::TextureTargetSet const&);

		static void ClearColorTargets(re::TextureTargetSet const&);
		static void ClearDepthStencilTarget(re::TextureTargetSet const&);

		static void AttachTargetsAsImageTextures(re::TextureTargetSet const&); // ~Compute target/UAV
	};
}