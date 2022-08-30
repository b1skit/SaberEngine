#pragma once

#include <memory>
#include <string>

#include "Shader.h"
#include "Camera.h"
#include "TextureTarget.h"


namespace gr
{
	class RenderStage
	{
	public:
		RenderStage(std::string name);
		~RenderStage() = default;
		
		RenderStage() = delete;
		RenderStage(RenderStage const&) = delete;
		RenderStage(RenderStage&&) = delete;
		RenderStage& operator=(RenderStage const&) = delete;
		
		inline std::string& GetName() { return m_name; }

		inline std::shared_ptr<gr::Shader>& GetStageShader() { return m_stageShader; }
		inline std::shared_ptr<gr::Shader const> GetStageShader() const { return m_stageShader; }

		inline std::shared_ptr<gr::Camera>& GetStageCamera() { return m_stageCam; }
		inline std::shared_ptr<gr::Camera const> GetStageCamera() const { return m_stageCam; }

		inline gr::TextureTargetSet& GetStageTargetSet() { return m_textureTargetSet; }
		inline gr::TextureTargetSet const& GetStageTargetSet() const { return m_textureTargetSet; }

	private:
		std::string m_name;
		std::shared_ptr<gr::Shader> m_stageShader;
		gr::TextureTargetSet m_textureTargetSet;
		std::shared_ptr<gr::Camera> m_stageCam;

		// TODO: Stages should maintain a list of (filtered) batches they'll draw
	};

}