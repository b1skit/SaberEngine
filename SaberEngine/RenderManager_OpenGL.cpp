#include <vector>

#include <GL/glew.h>
#include <GL/GL.h> // Must follow glew.h...

#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>

#include "RenderManager_OpenGL.h"
#include "RenderManager.h"
#include "RenderStage.h"
#include "TextureTarget.h"
#include "Shader.h"
#include "Camera.h"
#include "Transform.h"
#include "GraphicsSystem.h"
#include "GraphicsSystem_GBuffer.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "GraphicsSystem_Shadows.h"
#include "GraphicsSystem_Skybox.h"
#include "GraphicsSystem_Bloom.h"
#include "GraphicsSystem_Tonemapping.h"

using gr::RenderStage;
using gr::TextureTargetSet;
using gr::Shader;
using gr::Camera;
using gr::Transform;
using gr::GraphicsSystem;
using gr::GBufferGraphicsSystem;
using gr::ShadowsGraphicsSystem;
using gr::DeferredLightingGraphicsSystem;
using gr::SkyboxGraphicsSystem;
using gr::BloomGraphicsSystem;
using gr::TonemappingGraphicsSystem;
using std::shared_ptr;
using std::make_unique;
using std::make_shared;
using std::string;
using std::vector;
using glm::vec3;
using glm::vec4;
using glm::mat3;
using glm::mat4;


namespace opengl
{
	void RenderManager::Initialize(re::RenderManager& renderManager)
	{
		// Add graphics systems, in order:
		renderManager.m_graphicsSystems.emplace_back(make_shared<GBufferGraphicsSystem>("GBuffer Graphics System"));
		renderManager.m_graphicsSystems.emplace_back(make_shared<ShadowsGraphicsSystem>("Shadows Graphics System"));
		renderManager.m_graphicsSystems.emplace_back(make_shared<DeferredLightingGraphicsSystem>("Deferred Lighting Graphics System"));
		renderManager.m_graphicsSystems.emplace_back(make_shared<SkyboxGraphicsSystem>("Skybox Graphics System"));
		renderManager.m_graphicsSystems.emplace_back(make_shared<BloomGraphicsSystem>("Bloom Graphics System"));
		renderManager.m_graphicsSystems.emplace_back(make_shared<TonemappingGraphicsSystem>("Tonemapping Graphics System"));
		// NOTE: Adding a new graphics system? Don't forget to add a new template instantiation below GetGraphicsSystem()

		// Create each graphics system in turn:
		vector<shared_ptr<GraphicsSystem>>::iterator gsIt;
		for (gsIt = renderManager.m_graphicsSystems.begin(); gsIt != renderManager.m_graphicsSystems.end(); gsIt++)
		{
			(*gsIt)->Create(renderManager.m_pipeline.AddNewStagePipeline((*gsIt)->GetName()));

			// If the GS didn't attach any render stages, remove it
			if (renderManager.m_pipeline.GetPipeline().back().GetNumberOfStages() == 0)
			{
				renderManager.m_pipeline.GetPipeline().pop_back();
				vector<shared_ptr<GraphicsSystem>>::iterator deleteIt = gsIt;
				gsIt--;
				renderManager.m_graphicsSystems.erase(deleteIt);
			}
		}
	}


	void RenderManager::Render(re::RenderManager const& renderManager)
	{
		// TODO: Add an assert somewhere that checks if any possible shader uniform isn't set
		// -> Catch bugs where we forget to upload a common param

		// Update the graphics systems:
		for (std::shared_ptr<gr::GraphicsSystem> curGS : renderManager.m_graphicsSystems)
		{
			curGS->PreRender();
		}

		// Render each stage:
		const size_t numGS = renderManager.m_pipeline.GetNumberGraphicsSystems();
		for (size_t gsIdx = 0; gsIdx < numGS; gsIdx++)
		{
			// RenderDoc markers: Graphics system group name
			glPushDebugGroup(
				GL_DEBUG_SOURCE_APPLICATION, 
				0, 
				-1, 
				renderManager.m_pipeline.GetPipeline()[gsIdx].GetName().c_str());

			const size_t numStages = renderManager.m_pipeline.GetNumberOfGraphicsSystemStages(gsIdx);
			for (size_t stage = 0; stage < numStages; stage++)
			{
				RenderStage const* renderStage = renderManager.m_pipeline.GetPipeline()[gsIdx][stage];

				// RenderDoc makers: Render stage name
				glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, renderStage->GetName().c_str());

				RenderStage::RenderStageParams const& renderStageParams = renderStage->GetStageParams();

				// Attach the stage targets:
				TextureTargetSet const& stageTargets = renderStage->GetTextureTargetSet();
				stageTargets.AttachColorDepthStencilTargets(0, 0, true);
				// TODO: Handle selection of face, miplevel -> Via stage params?

				// Configure the shader:
				std::shared_ptr<Shader const> stageShader = renderStage->GetStageShader();
				stageShader->Bind(true);
				// TODO: Use shaders from materials in some cases?

				// Set per-frame stage shader uniforms:
				vector<RenderStage::StageShaderUniform> const& stagePerFrameShaderUniforms =
					renderStage->GetPerFrameShaderUniforms();
				for (RenderStage::StageShaderUniform curUniform : stagePerFrameShaderUniforms)
				{
					stageShader->SetUniform(
						curUniform.m_uniformName, curUniform.m_value, curUniform.m_type, curUniform.m_count);
				}

				// Set camera params:
				std::shared_ptr<Camera const> stageCam = renderStage->GetStageCamera();
				stageShader->SetUniform(
					"in_view",
					&stageCam->GetViewMatrix()[0][0],
					platform::Shader::UniformType::Matrix4x4f,
					1);
				stageShader->SetUniform(
					"cameraWPos",
					&stageCam->GetTransform()->GetWorldPosition(),
					platform::Shader::UniformType::Vec3f,
					1);
				// TODO: These should be set via a general camera param block, shared between stages that need it

				// Configure the context:
				renderManager.m_context.ClearTargets(renderStageParams.m_targetClearMode);
				renderManager.m_context.SetCullingMode(renderStageParams.m_faceCullingMode);
				renderManager.m_context.SetBlendMode(renderStageParams.m_srcBlendMode, renderStageParams.m_dstBlendMode);
				renderManager.m_context.SetDepthMode(renderStageParams.m_depthMode);

				// Render stage geometry:
				std::vector<std::shared_ptr<gr::Mesh>> const* meshes = renderStage->GetGeometryBatches();
				SEAssert("Stage does not have any geometry to render", meshes != nullptr);
				size_t meshIdx = 0;
				for (std::shared_ptr<gr::Mesh> mesh : *meshes)
				{
					mesh->Bind(true);

					shared_ptr<gr::Material> meshMaterial = mesh->MeshMaterial();
					if (meshMaterial != nullptr &&
						renderStageParams.m_stageType != RenderStage::RenderStageType::DepthOnly)
					{
						// TODO: Is there a more elegant way to handle this?
						meshMaterial->BindToShader(stageShader);
					}


					// TODO: Support instancing. For now, just upload per-mesh parameters as a workaround...
					std::vector<std::vector<RenderStage::StageShaderUniform>> const& perMeshUniforms =
						renderStage->GetPerMeshPerFrameShaderUniforms();
					if (perMeshUniforms.size() > 0)
					{
						for (size_t curUniform = 0; curUniform < perMeshUniforms[meshIdx].size(); curUniform++)
						{
							stageShader->SetUniform(
								perMeshUniforms[meshIdx][curUniform].m_uniformName,
								perMeshUniforms[meshIdx][curUniform].m_value,
								perMeshUniforms[meshIdx][curUniform].m_type,
								perMeshUniforms[meshIdx][curUniform].m_count);
						}
					}


					// Assemble and upload mesh-specific matrices:
					const mat4 model = mesh->GetTransform().Model();
					const mat4 modelRotation = mesh->GetTransform().Model(Transform::WorldRotation);
					const mat4 mv = stageCam->GetViewMatrix() * model;
					const mat4 mvp = stageCam->GetViewProjectionMatrix() * model;

					stageShader->SetUniform("in_model", &model[0][0], platform::Shader::UniformType::Matrix4x4f, 1);
					stageShader->SetUniform(
						"in_modelRotation", &modelRotation[0][0], platform::Shader::UniformType::Matrix4x4f, 1);
					// ^^ TODO: in_modelRotation isn't always used...
					stageShader->SetUniform("in_mv", &mv[0][0], platform::Shader::UniformType::Matrix4x4f, 1);
					stageShader->SetUniform("in_mvp", &mvp[0][0], platform::Shader::UniformType::Matrix4x4f, 1);
					// TODO: Figure out a more generic solution for this?

					// Draw!
					glDrawElements(
						GL_TRIANGLES,
						(GLsizei)mesh->NumIndices(),
						GL_UNSIGNED_INT,
						(void*)(0)); // (GLenum mode, GLsizei count, GLenum type, const GLvoid* indices);

					meshIdx++;
				} // meshes

				glPopDebugGroup();
			}

			glPopDebugGroup();
		}

		// Display the final frame:
		renderManager.m_context.SwapWindow();
	}
}