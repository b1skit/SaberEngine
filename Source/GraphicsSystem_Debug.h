// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "GraphicsSystem.h"


namespace gr
{
	class DebugGraphicsSystem final : public virtual GraphicsSystem
	{
	public:

		DebugGraphicsSystem();

		void Create(re::StagePipeline&);

		void PreRender();

		std::shared_ptr<re::TextureTargetSet const> GetFinalTextureTargetSet() const override;

		void ShowImGuiWindow() override;


	private:
		void CreateBatches() override;


	private:
		std::shared_ptr<re::RenderStage> m_debugStage;

		bool m_showWorldCoordinateAxis = false;
		float m_worldCoordinateAxisScale = 1.f;
		glm::vec3 m_xAxisColor = glm::vec3(1.f, 0.f, 0.f);
		glm::vec3 m_yAxisColor = glm::vec3(0.f, 1.f, 0.f);
		glm::vec3 m_zAxisColor = glm::vec3(0.f, 0.f, 1.f);

		bool m_showMeshCoordinateAxis = false;
		float m_meshCoordinateAxisScale = 1.f;

		bool m_showAllMeshBoundingBoxes = false;
		glm::vec3 m_meshBoundsColor = glm::vec3(1.f, 0.f, 0.f);

		bool m_showAllMeshPrimitiveBoundingBoxes = false;
		glm::vec3 m_meshPrimitiveBoundsColor = glm::vec3(0.f, 1.f, 0.f);
		
		bool m_showAllVertexNormals = false;
		float m_vertexNormalsScale = 1.f;
		glm::vec3 m_normalsColor = glm::vec3(0.f, 0.f, 1.f);
	};


	inline std::shared_ptr<re::TextureTargetSet const> DebugGraphicsSystem::GetFinalTextureTargetSet() const
	{
		return m_debugStage->GetTextureTargetSet();
	}
}

