// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "BatchManager.h"
#include "RenderDataManager.h"
#include "RenderObjectIDs.h"
#include "CameraRenderData.h"


namespace re
{
	class RenderSystem;
}

namespace gr
{
	class GraphicsSystem;


	class GraphicsSystemManager
	{
	public:
		GraphicsSystemManager(re::RenderSystem*);
		~GraphicsSystemManager() = default;

		void Destroy();

		void Create();
		void PreRender();

		void CreateAddGraphicsSystemByScriptName(char const* scriptName);
		void CreateAddGraphicsSystemByScriptName(std::string const& scriptName);

		gr::GraphicsSystem* GetGraphicsSystemByScriptName(char const* scriptName) const;
		gr::GraphicsSystem* GetGraphicsSystemByScriptName(std::string const& scriptName) const;


	public:
		gr::BatchManager const& GetBatchManager() const;
		gr::RenderDataManager const& GetRenderData() const;


	public:
		gr::RenderDataID GetActiveCameraRenderDataID() const;
		gr::Camera::RenderData const& GetActiveCameraRenderData() const;
		gr::Transform::RenderData const& GetActiveCameraTransformData() const;
		std::shared_ptr<re::Buffer> GetActiveCameraParams() const;

		void SetActiveCamera(gr::RenderDataID cameraRenderDataID, gr::TransformID cameraTransformID);


	public:
		bool ActiveAmbientLightHasChanged() const;
		bool HasActiveAmbientLight() const;
		gr::RenderDataID GetActiveAmbientLightID() const;

	
	public:
		void ShowImGuiWindow();
		void ShowImGuiRenderDataDebugWindow() const;


	private:
		void UpdateActiveAmbientLight();


	private:
		std::vector<std::unique_ptr<gr::GraphicsSystem>> m_graphicsSystems;
		std::map<std::string, size_t> m_scriptNameToIndex;

		gr::RenderDataManager const* m_renderData;
		gr::BatchManager const* m_batchManager;

		gr::RenderDataID m_activeCameraRenderDataID;
		gr::TransformID m_activeCameraTransformDataID;
		std::shared_ptr<re::Buffer> m_activeCameraParams;

		gr::RenderDataID m_activeAmbientLightRenderDataID;
		bool m_activeAmbientLightHasChanged;

		re::RenderSystem* const m_owningRenderSystem;


	private: // No copying allwoed
		GraphicsSystemManager(GraphicsSystemManager const&) = delete;
		GraphicsSystemManager(GraphicsSystemManager&&) = delete;
		GraphicsSystemManager& operator=(GraphicsSystemManager const&) = delete;
		GraphicsSystemManager& operator=(GraphicsSystemManager&&) = delete;

	};


	inline gr::BatchManager const& GraphicsSystemManager::GetBatchManager() const
	{
		return *m_batchManager;
	}


	inline gr::RenderDataManager const& GraphicsSystemManager::GetRenderData() const
	{
		return *m_renderData;
	}


	inline gr::RenderDataID GraphicsSystemManager::GetActiveCameraRenderDataID() const
	{
		return m_activeCameraRenderDataID;
	}
}

