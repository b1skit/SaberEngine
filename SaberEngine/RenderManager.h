// The Saber rendering engine

#pragma once

#include "EngineComponent.h"	// Base class

#include <string>

#include <GL/glew.h>
#include <SDL.h>

#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>
using glm::vec4;

#include "grMesh.h"


namespace SaberEngine
{
	// Pre-declarations:
	//------------------
	class Material;
	class Mesh;
	class Camera;
	class Shader;
	class Light;
	class Skybox;
	class PostFXManager;


	enum SHADER // Guaranteed shaders
	{
		SHADER_ERROR = 0,		// Displays hot pink
		SHADER_DEFAULT = 1,		// Lambert shader
	};

	
	class RenderManager : public EngineComponent
	{
	public:
		RenderManager() : EngineComponent("RenderManager") {}

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
		//void RenderReflectionProbe();

		void RenderToGBuffer(Camera* const renderCam);	// Note: renderCam MUST have an attached GBuffer

		void RenderForward(Camera* renderCam);

		void RenderDeferredLight(Light* deferredLight); // Note: FBO, viewport

		void RenderSkybox(Skybox* skybox);

		void BlitToScreen();
		void BlitToScreen(Material* srcMaterial, Shader* blitShader);

		void Blit(Material* srcMat, int srcTex, Material* dstMat, int dstTex, Shader* shaderOverride = nullptr);


		// Configuration:
		//---------------
		int m_xRes					= -1;
		int m_yRes					= -1;
		string m_windowTitle			= "Default SaberEngine window title";

		bool m_useForwardRendering	= false;

		vec4 m_windowClearColor		= vec4(0.0f, 0.0f, 0.0f, 0.0f);
		float m_depthClearColor		= 1.0f;
		
		// OpenGL components and settings:
		SDL_Window* m_glWindow		= 0;
		SDL_GLContext m_glContext		= 0;

		Material* m_outputMaterial	= nullptr;	// Deallocated in Shutdown()
		gr::Mesh* m_screenAlignedQuad		= nullptr;	// Deallocated in Shutdown()

		// PostFX:
		PostFXManager* m_postFXManager = nullptr;	// Deallocated in Shutdown()

		
		// Private member functions:
		//--------------------------

		// Clear the window and fill it with a color
		void ClearWindow(vec4 clearColor);

	};
}


