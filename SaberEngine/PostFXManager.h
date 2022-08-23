#pragma once

#include <string>
#include <memory>

#include "Mesh.h"
#include "TextureTarget.h"


// Pre-declarations:
namespace gr
{
	class Mesh;
	class Shader;
}


namespace SaberEngine
{
	enum BLUR_PASS
	{
		BLUR_SHADER_LUMINANCE_THRESHOLD,
		BLUR_SHADER_HORIZONTAL,
		BLUR_SHADER_VERTICAL,

		BLUR_SHADER_COUNT			// RESERVED: Number of blur passes in total
	};

	class PostFXManager
	{
	public:
		PostFXManager() {} // Must call Initialize() before this object can be used

		~PostFXManager();

		// Initialize PostFX. Must be called after the scene has been loaded and the RenderManager has finished initializing OpenGL
		void Initialize(gr::TextureTarget const& fxTarget);

		// Apply post processing. Modifies finalFrameShader to contain the shader required to blit the final image to screen
		void ApplyPostFX(std::shared_ptr<gr::Shader>& finalFrameShader);



	private:
		gr::TextureTargetSet m_outputTargetSet;

		const uint32_t NUM_DOWN_SAMPLES = 2;		// Scaling factor: We half the frame size this many times
		const uint32_t NUM_BLUR_PASSES = 3;		// How many pairs of horizontal + vertical blur passes to perform

		// TODO: Move these into individual stages owned by a single graphics system
		std::vector<std::shared_ptr<gr::Texture>> m_pingPongTextures;	// Deallocated in destructor
		std::vector<gr::TextureTargetSet> m_pingPongStageTargetSets;

		std::shared_ptr<gr::Shader> m_blitShader = nullptr;
		std::shared_ptr<gr::Shader> m_toneMapShader	= nullptr;
		std::shared_ptr<gr::Shader> m_blurShaders[BLUR_SHADER_COUNT];
		
		std::shared_ptr<gr::Mesh> m_screenAlignedQuad	= nullptr;	// Deallocated in destructor
	};
}


