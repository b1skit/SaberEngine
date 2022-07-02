// Member class of the RenderManager. Handles PostFX work

#include "PostFXManager.h"
#include "BuildConfiguration.h"
#include "CoreEngine.h"
#include "grMesh.h"
#include "RenderTexture.h"
#include "Shader.h"
#include "Camera.h"
#include "Material.h"
#include "RenderTexture.h"

#include <vector>


namespace SaberEngine
{
	PostFXManager::~PostFXManager()
	{
		if (m_pingPongTextures != nullptr)
		{
			for (int i = 0; i < NUM_DOWN_SAMPLES; i++)
			{
				m_pingPongTextures[i].Destroy();
			}
			delete [] m_pingPongTextures;
			m_pingPongTextures	= nullptr;
		}

		if (m_blitShader != nullptr)
		{
			m_blitShader->Destroy();
			delete m_blitShader;
			m_blitShader = nullptr;
		}

		if (m_toneMapShader != nullptr)
		{
			m_toneMapShader->Destroy();
			delete m_toneMapShader;
			m_toneMapShader = nullptr;
		}

		if (m_blurShaders != nullptr)
		{
			for (int i = 0; i < BLUR_SHADER_COUNT; i++)
			{
				if (m_blurShaders[i] != nullptr)
				{
					m_blurShaders[i]->Destroy();
					delete m_blurShaders[i];
					m_blurShaders[i] = nullptr;
				}				
			}
		}

		if (m_screenAlignedQuad != nullptr)
		{
			m_screenAlignedQuad->Destroy();
			delete m_screenAlignedQuad;
			m_screenAlignedQuad = nullptr;
		}		
	}

	void PostFXManager::Initialize(Material* outputMaterial)
	{
		// Cache the output material
		m_outputMaterial = outputMaterial;

		// Configure render buffers:
		m_pingPongTextures = new RenderTexture[NUM_DOWN_SAMPLES + 1]; // +1 so we have an extra RenderTexture to pingpong between at the lowest res

		int currentXRes = CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowXRes") / 2;
		int currentYRes = CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowYRes") / 2;

		for (int i = 0; i <= NUM_DOWN_SAMPLES; i++)
		{
			m_pingPongTextures[i] = RenderTexture
			(
				currentXRes,
				currentYRes,
				"PostFX_PingPongTexture_" + to_string(currentXRes) + "x" + to_string(currentYRes)
			);

			m_pingPongTextures[i].Format() = GL_RGBA;		// Note: Using 4 channels for future flexibility
			m_pingPongTextures[i].InternalFormat() = GL_RGBA32F;

			m_pingPongTextures[i].TextureMinFilter() = GL_LINEAR;
			m_pingPongTextures[i].TextureMaxFilter() = GL_LINEAR;

			m_pingPongTextures[i].AttachmentPoint() = GL_COLOR_ATTACHMENT0 + 0;

			m_pingPongTextures[i].ReadBuffer() = GL_COLOR_ATTACHMENT0 + 0;
			m_pingPongTextures[i].DrawBuffer() = GL_COLOR_ATTACHMENT0 + 0;

			// Assign and buffer texture:
			m_pingPongTextures[i].Buffer(RENDER_TEXTURE_0 + RENDER_TEXTURE_ALBEDO);

			// Don't halve the resolution for the last 2 iterations:
			if (i < NUM_DOWN_SAMPLES - 1)
			{
				currentXRes /= 2;
				currentYRes /= 2;
			}
		}


		// Configure shaders:
		vector<string> luminanceThresholdKeywords(1,	"BLUR_SHADER_LUMINANCE_THRESHOLD");
		vector<string> horizontalBlurKeywords(1,		"BLUR_SHADER_HORIZONTAL");
		vector<string> verticalBlurKeywords(1,			"BLUR_SHADER_VERTICAL");
		
		m_blurShaders[BLUR_SHADER_LUMINANCE_THRESHOLD]	= Shader::CreateShader(CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("blurShader"), &luminanceThresholdKeywords);
		m_blurShaders[BLUR_SHADER_HORIZONTAL]				= Shader::CreateShader(CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("blurShader"), &horizontalBlurKeywords);
		m_blurShaders[BLUR_SHADER_VERTICAL]				= Shader::CreateShader(CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("blurShader"), &verticalBlurKeywords);

		m_blitShader										= Shader::CreateShader(CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("blitShader"));
		m_toneMapShader									= Shader::CreateShader(CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("toneMapShader"));


		// Upload Shader parameters:
		m_toneMapShader->UploadUniform("exposure", &CoreEngine::GetSceneManager()->GetMainCamera()->Exposure(), UNIFORM_Float);

		// Upload the texel size for the SMALLEST pingpong textures:
		vec4 texelSize = m_pingPongTextures[NUM_DOWN_SAMPLES].TexelSize();
		m_blurShaders[BLUR_SHADER_HORIZONTAL]->UploadUniform("texelSize", &texelSize.x, UNIFORM_Vec4fv);
		m_blurShaders[BLUR_SHADER_VERTICAL]->UploadUniform("texelSize", &texelSize.x, UNIFORM_Vec4fv);


		m_screenAlignedQuad = new gr::Mesh	// TODO: Use the RenderManager's instead of duplicating it here?
		(
			gr::meshfactory::CreateQuad
			(
				vec3(-1.0f, 1.0f, 0.0f),	// TL
				vec3(1.0f, 1.0f, 0.0f),		// TR
				vec3(-1.0f, -1.0f, 0.0f),	// BL
				vec3(1.0f, -1.0f, 0.0f)		// BR
			)
		);
	}


	void PostFXManager::ApplyPostFX(Material*& finalFrameMaterial, Shader*& finalFrameShader)
	{
		// Pass 1: Apply luminance threshold: Finished frame -> 1/2 res
		gr::Mesh::Bind(*m_screenAlignedQuad, true);
		glViewport(0, 0, m_pingPongTextures[0].Width(), m_pingPongTextures[0].Height());

		// Bind the target FBO, luminance threshold shader, and source texture:
		m_pingPongTextures[0].BindFramebuffer(true);
		m_blurShaders[BLUR_SHADER_LUMINANCE_THRESHOLD]->Bind(true);
		m_outputMaterial->AccessTexture(RENDER_TEXTURE_ALBEDO)->Bind(RENDER_TEXTURE_0 + RENDER_TEXTURE_ALBEDO, true);

		// Draw!
		glDrawElements(
			GL_TRIANGLES, // GLenum mode
			(GLsizei)m_screenAlignedQuad->NumIndices(), //GLsizei count
			GL_UNSIGNED_INT, // GLenum type
			(void*)(0)); // const GLvoid* indices

		// Cleanup:
		m_blurShaders[BLUR_SHADER_LUMINANCE_THRESHOLD]->Bind(false);
		m_outputMaterial->AccessTexture(RENDER_TEXTURE_ALBEDO)->Bind(RENDER_TEXTURE_0 + RENDER_TEXTURE_ALBEDO, false);
		m_pingPongTextures[0].BindFramebuffer(false);

		// Continue downsampling: Blit to the remaining textures:
		m_blitShader->Bind(true);
		for (int i = 1; i < NUM_DOWN_SAMPLES; i++)
		{
			// Configure the viewport:
			glViewport(0, 0, m_pingPongTextures[i].Width(), m_pingPongTextures[i].Height());

			// Bind the target FBO, and source texture to the shader
			m_pingPongTextures[i].BindFramebuffer(true);
			m_pingPongTextures[i - 1].Bind(RENDER_TEXTURE_0 + RENDER_TEXTURE_ALBEDO, true);

			// Draw!
			glDrawElements(GL_TRIANGLES,
				(GLsizei)m_screenAlignedQuad->NumIndices(),
				GL_UNSIGNED_INT,
				(void*)(0)); // (GLenum mode, GLsizei count, GLenum type, const GLvoid* indices);

			// Cleanup:
			m_pingPongTextures[i - 1].Bind(RENDER_TEXTURE_0 + RENDER_TEXTURE_ALBEDO, false);
			m_pingPongTextures[i].BindFramebuffer(false);
		}

		// Cleanup:
		m_blitShader->Bind(false);

		// Blur the final low-res image:
		glViewport(0, 0, m_pingPongTextures[NUM_DOWN_SAMPLES].Width(), m_pingPongTextures[NUM_DOWN_SAMPLES].Height());
		for (int i = 0; i < NUM_BLUR_PASSES; i++)
		{
			// Horizontal pass: (NUM_DOWN_SAMPLES - 1) -> NUM_DOWN_SAMPLES

			// Bind the target FBO, shader, and source texture:
			m_pingPongTextures[NUM_DOWN_SAMPLES].BindFramebuffer(true);
			m_blurShaders[BLUR_SHADER_HORIZONTAL]->Bind(true);
			m_pingPongTextures[NUM_DOWN_SAMPLES - 1].Bind(RENDER_TEXTURE_0 + RENDER_TEXTURE_ALBEDO, true);

			// Draw!
			glDrawElements(GL_TRIANGLES, 
				(GLsizei)m_screenAlignedQuad->NumIndices(), 
				GL_UNSIGNED_INT,
				(void*)(0));

			// Cleanup:
			m_pingPongTextures[NUM_DOWN_SAMPLES - 1].Bind(RENDER_TEXTURE_0 + RENDER_TEXTURE_ALBEDO, false);
			m_blurShaders[BLUR_SHADER_HORIZONTAL]->Bind(false);
			m_pingPongTextures[NUM_DOWN_SAMPLES].BindFramebuffer(false);


			// Vertical pass: NUM_DOWN_SAMPLES -> (NUM_DOWN_SAMPLES - 1)
			
			// Bind the target FBO, shader, and source texture:
			m_pingPongTextures[NUM_DOWN_SAMPLES - 1].BindFramebuffer(true);
			m_blurShaders[BLUR_SHADER_VERTICAL]->Bind(true);
			m_pingPongTextures[NUM_DOWN_SAMPLES].Bind(RENDER_TEXTURE_0 + RENDER_TEXTURE_ALBEDO, true);

			// Draw!
			glDrawElements(GL_TRIANGLES,
				(GLsizei)m_screenAlignedQuad->NumIndices(),
				GL_UNSIGNED_INT, 
				(void*)(0));

			// Cleanup:
			m_pingPongTextures[NUM_DOWN_SAMPLES].Bind(RENDER_TEXTURE_0 + RENDER_TEXTURE_ALBEDO, false);
			m_blurShaders[BLUR_SHADER_VERTICAL]->Bind(false);
			m_pingPongTextures[NUM_DOWN_SAMPLES - 1].BindFramebuffer(false);
		}

		// Up-sample: Blit to successively larger textures:
		m_blitShader->Bind(true);
		for (int i = NUM_DOWN_SAMPLES - 1; i > 0; i--)
		{
			// Configure the viewport for the next, larger texture:
			glViewport(0, 0, m_pingPongTextures[i - 1].Width(), m_pingPongTextures[i - 1].Height());

			// Bind the target FBO, and source texture to the shader:
			m_pingPongTextures[i - 1].BindFramebuffer(true);
			m_pingPongTextures[i].Bind(RENDER_TEXTURE_0 + RENDER_TEXTURE_ALBEDO, true);

			// Draw!
			glDrawElements(GL_TRIANGLES, 
				(GLsizei)m_screenAlignedQuad->NumIndices(),
				GL_UNSIGNED_INT,
				(void*)(0));

			// Cleanup:
			m_pingPongTextures[i].Bind(RENDER_TEXTURE_0 + RENDER_TEXTURE_ALBEDO, false);
			m_pingPongTextures[i - 1].BindFramebuffer(false);
		}

		// Additively blit final blurred result (ie. half res) to the original, full-sized image: [0] -> output material
		glViewport(0, 0, m_outputMaterial->AccessTexture(RENDER_TEXTURE_ALBEDO)->Width(), m_outputMaterial->AccessTexture(RENDER_TEXTURE_ALBEDO)->Height());
		((RenderTexture*)m_outputMaterial->AccessTexture(RENDER_TEXTURE_ALBEDO))->BindFramebuffer(true);

		// Bind source:
		m_pingPongTextures[0].Bind(RENDER_TEXTURE_0 + RENDER_TEXTURE_ALBEDO, true);
		
		glEnable(GL_BLEND);
		glDrawElements(GL_TRIANGLES,
			(GLsizei)m_screenAlignedQuad->NumIndices(),
			GL_UNSIGNED_INT,
			(void*)(0)); // (GLenum mode, GLsizei count, GLenum type, const GLvoid* indices);
		glDisable(GL_BLEND);

		// Set the final frame material and shader to apply tone mapping:
		finalFrameMaterial	= m_outputMaterial;
		finalFrameShader	= m_toneMapShader;

		// Cleanup:
		m_blitShader->Bind(false);
		m_pingPongTextures[0].Bind(RENDER_TEXTURE_0 + RENDER_TEXTURE_ALBEDO, false);
		((RenderTexture*)m_outputMaterial->AccessTexture(RENDER_TEXTURE_ALBEDO))->BindFramebuffer(false);

		gr::Mesh::Bind(*m_screenAlignedQuad, false);
	}
}


