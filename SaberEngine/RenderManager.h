#pragma once

#include <string>
#include <memory>

#include <GL/glew.h>
#include <SDL.h>

#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>
using glm::vec4;

#include "EngineComponent.h"
#include "grMesh.h"
#include "grTextureTarget.h"
#include "reContext.h"


// Pre-declarations:
namespace gr
{
	class Mesh;
}
namespace SaberEngine
{
	class Material;
	class Camera;
	class Shader;
	class Light;
	class Skybox;
	class PostFXManager;
}


namespace SaberEngine
{
	enum SHADER // Guaranteed shaders
	{
		SHADER_ERROR = 0,		// Displays hot pink
		SHADER_DEFAULT = 1,		// Lambert shader
	};


	class RenderManager : public virtual EngineComponent
	{
	public:
		RenderManager() : EngineComponent("RenderManager") {};

		~RenderManager();

		// Singleton functionality:
		static RenderManager& Instance();
		RenderManager(RenderManager const&) = delete; // Disallow copying of our Singleton
		void operator=(RenderManager const&) = delete;

		// EngineComponent interface:
		void Startup();

		void Shutdown();

		void Update();

		
		// Member functions:
		//------------------

		// Perform post scene load initialization (eg. Upload static properties to shaders, initialize PostFX):
		void Initialize();


	private:
		void RenderLightShadowMap(Light* currentLight);

		void RenderToGBuffer(Camera* const renderCam);	// Note: renderCam MUST have an attached GBuffer

		void RenderForward(Camera* renderCam);

		void RenderDeferredLight(Light* deferredLight); // Note: FBO, viewport

		void RenderSkybox(Skybox* skybox);

		void BlitToScreen();
		void BlitToScreen(std::shared_ptr<gr::Texture>& texture, Shader* blitShader);

		void Blit(std::shared_ptr<gr::Texture> const& srcTex, gr::TextureTargetSet const& dstTargetSet, Shader* shader);


		// Configuration:
		//---------------
		int m_xRes					= -1;
		int m_yRes					= -1;
		// TODO: Make sure these don't get out of sync with our Context...
		
		re::Context m_context;

		bool m_useForwardRendering	= false; // TODO: Remove this functionality

		Material* m_outputMaterial	= nullptr;	// Deallocated in Shutdown()
		std::shared_ptr<gr::TextureTargetSet> m_outputTargetSet;

		std::shared_ptr<gr::Mesh> m_screenAlignedQuad		= nullptr;	// Deallocated in Shutdown()

		// PostFX:
		PostFXManager* m_postFXManager = nullptr;	// Deallocated in Shutdown()
	};
}


