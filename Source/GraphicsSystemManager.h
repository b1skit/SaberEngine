// � 2023 Adam Badke. All rights reserved.
#pragma once
#include "Assert.h"
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
		T* GetGraphicsSystem() const;

		std::vector<std::shared_ptr<gr::GraphicsSystem>>& GetGraphicsSystems();

		
	public: // Batch manager:
		std::vector<re::Batch> GetVisibleBatches(
			gr::Camera::View const&,
			uint8_t bufferTypeMask = (gr::BatchManager::InstanceType::Transform | gr::BatchManager::InstanceType::Material)) const;

		std::vector<re::Batch> GetVisibleBatches(
			std::vector<gr::Camera::View> const&, 
			uint8_t bufferTypeMask = (gr::BatchManager::InstanceType::Transform | gr::BatchManager::InstanceType::Material)) const;

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
		// Not thread safe: Can only be called when other threads are not accessing the render data
		gr::RenderDataManager& GetRenderDataForModification();

	
	public:
		void ShowImGuiWindow();
		void ShowImGuiRenderDataDebugWindow() const;


	private:
		void UpdateActiveAmbientLight();


	private:
		std::vector<std::shared_ptr<gr::GraphicsSystem>> m_graphicsSystems;

		gr::RenderDataManager m_renderData;
		gr::BatchManager m_batchManager;

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


	inline std::vector<std::shared_ptr<gr::GraphicsSystem>>& GraphicsSystemManager::GetGraphicsSystems()
	{
		return m_graphicsSystems;
	}


	template <typename T>
	T* GraphicsSystemManager::GetGraphicsSystem() const
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


	inline gr::RenderDataID GraphicsSystemManager::GetActiveCameraRenderDataID() const
	{
		return m_activeCameraRenderDataID;
	}


	inline gr::RenderDataManager& GraphicsSystemManager::GetRenderDataForModification()
	{
		return m_renderData;
	}
}

