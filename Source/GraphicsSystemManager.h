// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Assert.h"
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
	class CommandBuffer;

	class GraphicsSystemManager
	{
	public:
		GraphicsSystemManager(re::RenderSystem*);
		~GraphicsSystemManager() = default;

		void Destroy();

		void Create();
		void PreRender();

		template <typename T>
		T* GetGraphicsSystem();

		std::vector<std::shared_ptr<gr::GraphicsSystem>>& GetGraphicsSystems();

		gr::RenderDataManager const& GetRenderData() const;

		gr::Camera::RenderData const& GetActiveCameraRenderData() const;
		gr::Transform::RenderData const& GetActiveCameraTransformData() const;
		std::shared_ptr<re::ParameterBlock> GetActiveCameraParams() const;

		// Not thread safe: Can only be called when other threads are not accessing the render data
		gr::RenderDataManager& GetRenderDataForModification();
		void SetActiveCamera(gr::RenderDataID cameraRenderDataID, gr::TransformID cameraTransformID);

		void ShowImGuiWindow();
		void ShowImGuiRenderDataDebugWindow() const;


	private:
		std::vector<std::shared_ptr<gr::GraphicsSystem>> m_graphicsSystems;

		gr::RenderDataManager m_renderData;

		gr::RenderDataID m_activeCameraRenderDataID;
		gr::TransformID m_activeCameraTransformDataID;
		std::shared_ptr<re::ParameterBlock> m_activeCameraParams;

		re::RenderSystem* const m_owningRenderSystem;


	private: // No copying allwoed
		GraphicsSystemManager(GraphicsSystemManager const&) = delete;
		GraphicsSystemManager(GraphicsSystemManager&&) = delete;
		GraphicsSystemManager& operator=(GraphicsSystemManager const&) = delete;
		GraphicsSystemManager& operator=(GraphicsSystemManager&&) = delete;

	};


	inline std::vector<std::shared_ptr<gr::GraphicsSystem>>& GraphicsSystemManager::GetGraphicsSystems()
	{
		return m_graphicsSystems;
	}


	template <typename T>
	T* GraphicsSystemManager::GetGraphicsSystem()
	{
		// TODO: A linear search isn't optimal here, but there aren't many graphics systems in practice so ok for now
		for (size_t i = 0; i < m_graphicsSystems.size(); i++)
		{
			T* result = dynamic_cast<T*>(m_graphicsSystems[i].get());
			if (result != nullptr)
			{
				return result;
			}
		}

		return nullptr;
	}


	inline gr::RenderDataManager const& GraphicsSystemManager::GetRenderData() const
	{
		return m_renderData;
	}


	inline gr::RenderDataManager& GraphicsSystemManager::GetRenderDataForModification()
	{
		return m_renderData;
	}
}

