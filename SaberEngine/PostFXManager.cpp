#include <vector>
#include <memory>

#include "PostFXManager.h"
#include "BuildConfiguration.h"
#include "CoreEngine.h"
#include "Mesh.h"
#include "Shader.h"
#include "Camera.h"
#include "Material.h"


namespace SaberEngine
{
	PostFXManager::~PostFXManager()
	{
		for (size_t i = 0; i < m_pingPongTextures.size(); i++)
		{
			m_pingPongTextures[i] = nullptr;
		}

		m_blitShader = nullptr;
		m_toneMapShader = nullptr;

		if (m_blurShaders != nullptr)
		{
			for (int i = 0; i < BLUR_SHADER_COUNT; i++)
			{
				m_blurShaders[i] = nullptr;
			}
		}

		if (m_screenAlignedQuad != nullptr)
		{
			m_screenAlignedQuad->Destroy();
			m_screenAlignedQuad = nullptr;
		}		
	}

	void PostFXManager::Initialize(Material* outputMaterial)
	{				
		m_outputTargetSet.ColorTarget(0) = outputMaterial->AccessTexture(RENDER_TEXTURE_ALBEDO);

		m_outputTargetSet.CreateColorTargets(RENDER_TEXTURE_0 + RENDER_TEXTURE_ALBEDO);

		// Configure texture targets. 
		const uint32_t numStages = NUM_DOWN_SAMPLES + 1; // +1 so we can ping-pong between at the lowest res
		m_pingPongStageTargetSets = std::vector<gr::TextureTargetSet>(numStages);

		int currentXRes = CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowXRes") / 2;
		int currentYRes = CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowYRes") / 2;

		gr::Texture::TextureParams pingPongParams;
		pingPongParams.m_width = currentXRes;
		pingPongParams.m_height = currentYRes;
		pingPongParams.m_faces = 1;
		pingPongParams.m_texUse = gr::Texture::TextureUse::ColorTarget;
		pingPongParams.m_texDimension = gr::Texture::TextureDimension::Texture2D;
		pingPongParams.m_texFormat = gr::Texture::TextureFormat::RGBA32F;
		pingPongParams.m_texColorSpace = gr::Texture::TextureColorSpace::Linear;
		pingPongParams.m_texSamplerMode = gr::Texture::TextureSamplerMode::Clamp; // Wrap produces artifacts at image borders
		pingPongParams.m_texMinMode = gr::Texture::TextureMinFilter::Linear;
		pingPongParams.m_texMaxMode = gr::Texture::TextureMaxFilter::Linear;
		pingPongParams.m_clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);

		for (uint32_t i = 0; i < numStages; i++)
		{
			pingPongParams.m_width = currentXRes;
			pingPongParams.m_height = currentYRes;
			pingPongParams.m_texturePath = 
				"PostFX_PingPongTexture_" + to_string(currentXRes) + "x" + to_string(currentYRes);

			m_pingPongStageTargetSets[i].ColorTarget(0) = std::shared_ptr<gr::Texture>(new gr::Texture(pingPongParams));
			
			m_pingPongStageTargetSets[i].Viewport().xMin() = 0;
			m_pingPongStageTargetSets[i].Viewport().yMin() = 0;
			m_pingPongStageTargetSets[i].Viewport().Width() = currentXRes;
			m_pingPongStageTargetSets[i].Viewport().Height() = currentYRes;

			// TODO: Bind function should call CreateColorTargets internally
			m_pingPongStageTargetSets[i].CreateColorTargets(RENDER_TEXTURE_0 + RENDER_TEXTURE_ALBEDO);		

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
		
		m_blurShaders[BLUR_SHADER_LUMINANCE_THRESHOLD] = Shader::CreateShader(
			CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("blurShader"), &luminanceThresholdKeywords);

		m_blurShaders[BLUR_SHADER_HORIZONTAL] = Shader::CreateShader(
			CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("blurShader"), &horizontalBlurKeywords);

		m_blurShaders[BLUR_SHADER_VERTICAL]	= Shader::CreateShader(
			CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("blurShader"), &verticalBlurKeywords);

		m_blitShader = Shader::CreateShader(
			CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("blitShader"));

		m_toneMapShader	= Shader::CreateShader(
			CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("toneMapShader"));


		// Upload Shader parameters:
		m_toneMapShader->UploadUniform(
			"exposure", &CoreEngine::GetSceneManager()->GetMainCamera()->Exposure(), UNIFORM_Float);

		// Upload the texel size for the SMALLEST pingpong textures:
		
		/*const vec4 texelSize = m_pingPongTextures[NUM_DOWN_SAMPLES]->GetTexelDimenions();*/
		const vec4 smallestTexelSize = 
			m_pingPongStageTargetSets[NUM_DOWN_SAMPLES].ColorTarget(0).GetTexture()->GetTexelDimenions();

		m_blurShaders[BLUR_SHADER_HORIZONTAL]->UploadUniform("texelSize", &smallestTexelSize.x, UNIFORM_Vec4fv);
		m_blurShaders[BLUR_SHADER_VERTICAL]->UploadUniform("texelSize", &smallestTexelSize.x, UNIFORM_Vec4fv);

		// TODO: Use the RenderManager's instead of duplicating it here?
		m_screenAlignedQuad = gr::meshfactory::CreateQuad
		(
			vec3(-1.0f, 1.0f, 0.0f),	// TL
			vec3(1.0f, 1.0f, 0.0f),		// TR
			vec3(-1.0f, -1.0f, 0.0f),	// BL
			vec3(1.0f, -1.0f, 0.0f)		// BR
		);
	}


	void PostFXManager::ApplyPostFX(std::shared_ptr<Shader>& finalFrameShader)
	{
		// Pass 1: Apply luminance threshold: Finished frame -> 1/2 res

		m_pingPongStageTargetSets[0].AttachColorTargets(0, 0, true);

		m_outputTargetSet.ColorTarget(0).GetTexture()->Bind(RENDER_TEXTURE_0 + RENDER_TEXTURE_ALBEDO, true);

		m_screenAlignedQuad->Bind(true);
		m_blurShaders[BLUR_SHADER_LUMINANCE_THRESHOLD]->Bind(true);

		// Draw
		glDrawElements(
			GL_TRIANGLES,								// GLenum mode
			(GLsizei)m_screenAlignedQuad->NumIndices(), //GLsizei count
			GL_UNSIGNED_INT,							// GLenum type
			(void*)(0));								// const GLvoid* indices


		// Continue downsampling: Blit to the remaining textures:
		m_blitShader->Bind(true);

		for (uint32_t i = 1; i < NUM_DOWN_SAMPLES; i++)
		{
			m_pingPongStageTargetSets[i].AttachColorTargets(0, 0, true);

			m_pingPongStageTargetSets[i - 1].ColorTarget(0).GetTexture()->Bind(
				RENDER_TEXTURE_0 + RENDER_TEXTURE_ALBEDO, 
				true);
			

			// Draw!
			glDrawElements(GL_TRIANGLES,
				(GLsizei)m_screenAlignedQuad->NumIndices(),
				GL_UNSIGNED_INT,
				(void*)(0)); // (GLenum mode, GLsizei count, GLenum type, const GLvoid* indices);
		}

		// Blur the final low-res image:
		for (uint32_t i = 0; i < NUM_BLUR_PASSES; i++)
		{
			// Horizontal pass: (NUM_DOWN_SAMPLES - 1) -> NUM_DOWN_SAMPLES
			m_pingPongStageTargetSets[NUM_DOWN_SAMPLES].AttachColorTargets(0, 0, true);

			m_pingPongStageTargetSets[NUM_DOWN_SAMPLES - 1].ColorTarget(0).GetTexture()->Bind(
				RENDER_TEXTURE_0 + RENDER_TEXTURE_ALBEDO,
				true);

			m_blurShaders[BLUR_SHADER_HORIZONTAL]->Bind(true);

			// Draw!
			glDrawElements(GL_TRIANGLES, 
				(GLsizei)m_screenAlignedQuad->NumIndices(), 
				GL_UNSIGNED_INT,
				(void*)(0));


			// Vertical pass: NUM_DOWN_SAMPLES -> (NUM_DOWN_SAMPLES - 1)
			m_pingPongStageTargetSets[NUM_DOWN_SAMPLES - 1].AttachColorTargets(0, 0, true);

			m_pingPongStageTargetSets[NUM_DOWN_SAMPLES].ColorTarget(0).GetTexture()->Bind(
				RENDER_TEXTURE_0 + RENDER_TEXTURE_ALBEDO,
				true);

			m_blurShaders[BLUR_SHADER_VERTICAL]->Bind(true);

			// Draw!
			glDrawElements(GL_TRIANGLES,
				(GLsizei)m_screenAlignedQuad->NumIndices(),
				GL_UNSIGNED_INT, 
				(void*)(0));
		}

		// Up-sample: Blit to successively larger textures:
		m_blitShader->Bind(true);
		for (int i = NUM_DOWN_SAMPLES - 1; i > 0; i--)
		{
			m_pingPongStageTargetSets[i - 1].AttachColorTargets(0, 0, true);

			m_pingPongStageTargetSets[i].ColorTarget(0).GetTexture()->Bind(
				RENDER_TEXTURE_0 + RENDER_TEXTURE_ALBEDO,
				true);

			// Draw!
			glDrawElements(GL_TRIANGLES, 
				(GLsizei)m_screenAlignedQuad->NumIndices(),
				GL_UNSIGNED_INT,
				(void*)(0));
		}

		// Additively blit final blurred result (ie. half res) to the original, full-sized image: [0] -> output material
		m_outputTargetSet.AttachColorTargets(0, 0, true);
		
		m_pingPongStageTargetSets[0].ColorTarget(0).GetTexture()->Bind(
			RENDER_TEXTURE_0 + RENDER_TEXTURE_ALBEDO,
			true);

		
		glEnable(GL_BLEND);
		glDrawElements(GL_TRIANGLES,
			(GLsizei)m_screenAlignedQuad->NumIndices(),
			GL_UNSIGNED_INT,
			(void*)(0)); // (GLenum mode, GLsizei count, GLenum type, const GLvoid* indices);
		glDisable(GL_BLEND);

		// Set the final frame material and shader to apply tone mapping:
		finalFrameShader	= m_toneMapShader;
	}
}


