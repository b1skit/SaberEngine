#pragma once

#include <memory>
#include <string>

#include "Shader.h"
#include "Shader_Platform.h"
#include "Camera.h"
#include "TextureTarget.h"
#include "Context_Platform.h"
#include "Mesh.h"


namespace gr
{
	class RenderStage
	{
	public:
		struct StageShaderUniform
		{
			std::string m_uniformName;
			void const* m_value;
			platform::Shader::UniformType const m_type;
			int m_count;
		};

		enum class RenderStageType
		{
			ColorOnly,
			DepthOnly,
			ColorAndDepth,

			RenderStageType_Count
		};

		struct RenderStageParams // Platform/context configuration:
		{			
			platform::Context::ClearTarget		m_targetClearMode	= platform::Context::ClearTarget::None;
			platform::Context::FaceCullingMode	m_faceCullingMode	= platform::Context::FaceCullingMode::Back;
			platform::Context::BlendMode		m_srcBlendMode		= platform::Context::BlendMode::One;
			platform::Context::BlendMode		m_dstBlendMode		= platform::Context::BlendMode::One;
			platform::Context::DepthTestMode	m_depthTestMode		= platform::Context::DepthTestMode::GEqual;
			platform::Context::DepthWriteMode	m_depthWriteMode	= platform::Context::DepthWriteMode::Enabled;

			RenderStageType						m_stageType			= RenderStageType::ColorAndDepth;
		};


	public:
		RenderStage(std::string const& name);
		~RenderStage() = default;
		
		RenderStage() = delete;
		RenderStage(RenderStage const&) = delete;
		RenderStage(RenderStage&&) = default;
		RenderStage& operator=(RenderStage const&) = delete;
		
		inline std::string const& GetName() const { return m_name; }

		void InitializeForNewFrame(); // Clears per-frame data

		inline void SetStageParams(RenderStageParams const& params) { m_stageParams = params; }
		inline RenderStageParams const& GetStageParams() const { return m_stageParams; }

		inline std::shared_ptr<gr::Shader>& GetStageShader() { return m_stageShader; }
		inline std::shared_ptr<gr::Shader const> GetStageShader() const { return m_stageShader; }

		inline std::shared_ptr<gr::Camera>& GetStageCamera() { return m_stageCam; }
		inline std::shared_ptr<gr::Camera const> GetStageCamera() const { return m_stageCam; }

		inline gr::TextureTargetSet& GetTextureTargetSet() { return m_textureTargetSet; }
		inline gr::TextureTargetSet const& GetTextureTargetSet() const { return m_textureTargetSet; }

		// Helper: Simultaneously binds a texture and sampler by name to the stage shader
		void SetTextureInput(
			std::string const& shaderName, std::shared_ptr<gr::Texture const> tex, std::shared_ptr<gr::Sampler const> sampler);

		// Per-frame uniforms are set every frame
		void SetPerFrameShaderUniformByPtr(
			std::string const& uniformName, void const* value, platform::Shader::UniformType const& type, int const count);
		// TODO: Maybe all uniforms should be set by value? Too easy to mistakenly set a pointer to a local variable...

		inline std::vector<StageShaderUniform> const& GetPerFrameShaderUniforms() const { return m_perFrameShaderUniforms; }

		template <typename T>
		void SetPerFrameShaderUniformByValue(
			std::string const& uniformName, T const& value, platform::Shader::UniformType const& type, int const count);

		inline std::vector<std::shared_ptr<gr::Mesh>> const* GetGeometryBatches() const { return m_stageGeometryBatches; }
		inline void SetGeometryBatches(std::vector<std::shared_ptr<gr::Mesh>> const* batches) { m_stageGeometryBatches = batches; }



		// TODO: Support instancing. For now, just use a vector of per-mesh uniforms with indexes to correspond to the
		// entreis in m_stageGeometryBatches
		void SetPerMeshPerFrameShaderUniformByPtr(
			size_t meshIdx, std::string const& uniformName, void const* value, platform::Shader::UniformType const& type, int const count);

		template <typename T>
		void SetPerMeshPerFrameShaderUniformByValue(
			size_t meshIdx, std::string const& uniformName, T const& value, platform::Shader::UniformType const& type, int const count);

		std::vector<std::vector<StageShaderUniform>> const& GetPerMeshPerFrameShaderUniforms() const { return m_perMeshShaderUniforms; }


	private:
		std::string const m_name;

		std::shared_ptr<gr::Shader> m_stageShader;
		gr::TextureTargetSet m_textureTargetSet;
		std::shared_ptr<gr::Camera> m_stageCam;
		
		RenderStageParams m_stageParams;

		// Per-frame members are cleared every frame
		std::vector<StageShaderUniform> m_perFrameShaderUniforms; // TODO: Handle selection of face, miplevel when binding color/depth targets?
		std::vector<std::shared_ptr<void>> m_perFrameShaderUniformValues; // Generic, per-frame data storage buffer
		
		// TODO: Implement a "m_stageConstantShaderUniforms" -> Things like textures, samplers, etc that don't change
		// between frames

		// TODO: Batches should be a slimmed-down version of everything we need to draw, rather than pointers to gr:: objects
		std::vector<std::shared_ptr<gr::Mesh>> const* m_stageGeometryBatches;


		// TEMP HAX: Shader uniforms for point lights, until I write an instancing solution
		std::vector<std::vector<StageShaderUniform>> m_perMeshShaderUniforms;
		
	};

}