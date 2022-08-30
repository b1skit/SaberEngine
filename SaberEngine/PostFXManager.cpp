#include <vector>
#include <memory>

#include "PostFXManager.h"
#include "DebugConfiguration.h"
#include "CoreEngine.h"
#include "Mesh.h"
#include "Shader.h"
#include "Camera.h"
#include "Sampler.h"
using gr::Material;
using gr::Texture;
using gr::Shader;
using gr::Sampler;
using std::shared_ptr;
using std::make_shared;
using glm::vec3;


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

		m_screenAlignedQuad = nullptr;
	}

	void PostFXManager::Initialize(gr::TextureTarget const& fxTarget)
	{				
		m_outputTargetSet.ColorTarget(0) = fxTarget;

		m_outputTargetSet.CreateColorTargets(Material::GBufferAlbedo);

		// Configure texture targets. 
		const uint32_t numStages = NUM_DOWN_SAMPLES + 1; // +1 so we can ping-pong between at the lowest res
		m_pingPongStageTargetSets = std::vector<gr::TextureTargetSet>(numStages);

		int currentXRes = CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowXRes") / 2;
		int currentYRes = CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowYRes") / 2;

		Texture::TextureParams pingPongParams;
		pingPongParams.m_width = currentXRes;
		pingPongParams.m_height = currentYRes;
		pingPongParams.m_faces = 1;
		pingPongParams.m_texUse = Texture::TextureUse::ColorTarget;
		pingPongParams.m_texDimension = Texture::TextureDimension::Texture2D;
		pingPongParams.m_texFormat = Texture::TextureFormat::RGBA32F;
		pingPongParams.m_texColorSpace = Texture::TextureColorSpace::Linear;
		pingPongParams.m_clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
		pingPongParams.m_useMIPs = false;

		for (uint32_t i = 0; i < numStages; i++)
		{
			pingPongParams.m_width = currentXRes;
			pingPongParams.m_height = currentYRes;
			pingPongParams.m_texturePath = 
				"PostFX_PingPongTexture_" + to_string(currentXRes) + "x" + to_string(currentYRes);

			m_pingPongStageTargetSets[i].ColorTarget(0) = std::make_shared<gr::Texture>(pingPongParams);
			
			m_pingPongStageTargetSets[i].Viewport().xMin() = 0;
			m_pingPongStageTargetSets[i].Viewport().yMin() = 0;
			m_pingPongStageTargetSets[i].Viewport().Width() = currentXRes;
			m_pingPongStageTargetSets[i].Viewport().Height() = currentYRes;

			// TODO: Bind function should call CreateColorTargets internally
			m_pingPongStageTargetSets[i].CreateColorTargets(Material::GBufferAlbedo);

			// Don't halve the resolution for the last 2 iterations:
			if (i < NUM_DOWN_SAMPLES - 1)
			{
				currentXRes /= 2;
				currentYRes /= 2;
			}
		}

		// Configure shaders:
		m_blurShaders[BLUR_SHADER_LUMINANCE_THRESHOLD] = make_shared<Shader>(
			CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("blurShader"));
		m_blurShaders[BLUR_SHADER_LUMINANCE_THRESHOLD]->ShaderKeywords().emplace_back("BLUR_SHADER_LUMINANCE_THRESHOLD");
		m_blurShaders[BLUR_SHADER_LUMINANCE_THRESHOLD]->Create();

		m_blurShaders[BLUR_SHADER_HORIZONTAL] = make_shared<Shader>(
			CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("blurShader"));
		m_blurShaders[BLUR_SHADER_HORIZONTAL]->ShaderKeywords().emplace_back("BLUR_SHADER_HORIZONTAL");
		m_blurShaders[BLUR_SHADER_HORIZONTAL]->Create();

		m_blurShaders[BLUR_SHADER_VERTICAL]	= make_shared<Shader>(
			CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("blurShader"));
		m_blurShaders[BLUR_SHADER_VERTICAL]->ShaderKeywords().emplace_back("BLUR_SHADER_VERTICAL");
		m_blurShaders[BLUR_SHADER_VERTICAL]->Create();

		m_blitShader = make_shared<Shader>(
			CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("blitShader"));
		m_blitShader->Create();

		m_toneMapShader	= make_shared<Shader>(
			CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("toneMapShader"));
		m_toneMapShader->Create();


		// Upload Shader parameters:
		m_toneMapShader->SetUniform(
			"exposure", &CoreEngine::GetSceneManager()->GetMainCamera()->GetExposure(), platform::Shader::UNIFORM_TYPE::Float);

		// Upload the texel size for the SMALLEST pingpong textures:
		const vec4 smallestTexelSize = 
			m_pingPongStageTargetSets[NUM_DOWN_SAMPLES].ColorTarget(0).GetTexture()->GetTexelDimenions();

		m_blurShaders[BLUR_SHADER_HORIZONTAL]->SetUniform("texelSize", 
			&smallestTexelSize.x, 
			platform::Shader::UNIFORM_TYPE::Vec4f);

		m_blurShaders[BLUR_SHADER_VERTICAL]->SetUniform("texelSize", 
			&smallestTexelSize.x, 
			platform::Shader::UNIFORM_TYPE::Vec4f);

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

		m_outputTargetSet.ColorTarget(0).GetTexture()->Bind(Material::GBufferAlbedo, true);
		Sampler::GetSampler(Sampler::SamplerType::ClampLinearLinear)->Bind(Material::GBufferAlbedo, true);

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
				Material::GBufferAlbedo,
				true);
			
			Sampler::GetSampler(Sampler::SamplerType::ClampLinearLinear)->Bind(Material::GBufferAlbedo, true);

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
				Material::GBufferAlbedo,
				true);

			Sampler::GetSampler(Sampler::SamplerType::ClampLinearLinear)->Bind(Material::GBufferAlbedo, true);

			m_blurShaders[BLUR_SHADER_HORIZONTAL]->Bind(true);

			// Draw!
			glDrawElements(GL_TRIANGLES, 
				(GLsizei)m_screenAlignedQuad->NumIndices(), 
				GL_UNSIGNED_INT,
				(void*)(0));


			// Vertical pass: NUM_DOWN_SAMPLES -> (NUM_DOWN_SAMPLES - 1)
			m_pingPongStageTargetSets[NUM_DOWN_SAMPLES - 1].AttachColorTargets(0, 0, true);

			m_pingPongStageTargetSets[NUM_DOWN_SAMPLES].ColorTarget(0).GetTexture()->Bind(
				Material::GBufferAlbedo,
				true);

			Sampler::GetSampler(Sampler::SamplerType::ClampLinearLinear)->Bind(Material::GBufferAlbedo, true);

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
				Material::GBufferAlbedo,
				true);

			Sampler::GetSampler(Sampler::SamplerType::ClampLinearLinear)->Bind(Material::GBufferAlbedo, true);

			// Draw!
			glDrawElements(GL_TRIANGLES, 
				(GLsizei)m_screenAlignedQuad->NumIndices(),
				GL_UNSIGNED_INT,
				(void*)(0));
		}

		// Additively blit final blurred result (ie. half res) to the original, full-sized image: [0] -> output
		m_outputTargetSet.AttachColorTargets(0, 0, true);
		
		m_pingPongStageTargetSets[0].ColorTarget(0).GetTexture()->Bind(
			Material::GBufferAlbedo,
			true);

		CoreEngine::GetRenderManager()->GetContext().SetBlendMode(
			platform::Context::BlendMode::One,
			platform::Context::BlendMode::One);

		glDrawElements(GL_TRIANGLES,
			(GLsizei)m_screenAlignedQuad->NumIndices(),
			GL_UNSIGNED_INT,
			(void*)(0)); // (GLenum mode, GLsizei count, GLenum type, const GLvoid* indices);

		CoreEngine::GetRenderManager()->GetContext().SetBlendMode(
			platform::Context::BlendMode::Disabled,
			platform::Context::BlendMode::Disabled);

		// Set the final frame shader to apply tone mapping:
		finalFrameShader = m_toneMapShader;
	}
}


