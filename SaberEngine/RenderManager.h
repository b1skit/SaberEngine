#pragma once

#include <string>
#include <memory>

#include <GL/glew.h>
#include <SDL.h>

#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>
using glm::vec4;

#include "EngineComponent.h"
#include "Mesh.h"
#include "TextureTarget.h"
#include "Context.h"
#include "PostFXManager.h"
#include "Context_Platform.h"


// Pre-declarations:
namespace gr
{
	class Mesh;
	class Shader;
}
namespace SaberEngine
{
	class Camera;
	class Light;
	class Skybox;
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

		re::Context const& GetContext() { return m_context; }

	private:
		void RenderLightShadowMap(std::shared_ptr<Light> currentLight);

		void RenderToGBuffer(std::shared_ptr<Camera> const renderCam);	// Note: renderCam MUST have an attached GBuffer

		void RenderDeferredLight(std::shared_ptr<Light> deferredLight); // Note: FBO, viewport

		void RenderSkybox(std::shared_ptr<Skybox> skybox);

		void BlitToScreen();
		void BlitToScreen(std::shared_ptr<gr::Texture>& texture, std::shared_ptr<gr::Shader> blitShader);

		void Blit(std::shared_ptr<gr::Texture> const& srcTex, gr::TextureTargetSet const& dstTargetSet, std::shared_ptr<gr::Shader> shader);


		// Configuration:
		//---------------
		int m_xRes					= -1;
		int m_yRes					= -1;
		// TODO: Make sure these don't get out of sync with our Context...
		
		re::Context m_context;

		std::shared_ptr<gr::Shader> m_blitShader = nullptr;

		// Note: We store these as shared_ptr so we can instantiate them once the context has been created
		std::shared_ptr<gr::TextureTargetSet> m_outputTargetSet = nullptr; // TODO: Pick a better name for this...
		std::shared_ptr<gr::TextureTargetSet> m_defaultTargetSet = nullptr;

		std::shared_ptr<gr::Mesh> m_screenAlignedQuad = nullptr;

		std::unique_ptr<PostFXManager> m_postFXManager = nullptr;
		
		// TODO: Move initialization to ctor init list
	};
}


