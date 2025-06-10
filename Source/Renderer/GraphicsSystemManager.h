// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "BufferView.h"
#include "RenderDataManager.h"
#include "RenderObjectIDs.h"


namespace gr
{
	class GraphicsSystem;
	class RenderSystem;


	class GraphicsSystemManager
	{
	public:
		GraphicsSystemManager(gr::RenderSystem*);
		~GraphicsSystemManager() = default;

		void Destroy();

		void Create();
		void PreRender();

		void CreateAddGraphicsSystemByScriptName(std::string_view scriptName);

		// NOTE: Accessing GraphicsSystems is generally NOT thread safe. These "GetGraphicsSystem" functions are
		// provided as a convenience for initial setup only
		gr::GraphicsSystem* GetGraphicsSystemByScriptName(char const* scriptName) const;
		gr::GraphicsSystem* GetGraphicsSystemByScriptName(std::string const& scriptName) const;

		template<typename T>
		T* GetGraphicsSystem() const;


	public:
		void EndOfFrame();


	public:
		gr::RenderDataManager const& GetRenderData() const;


	public:
		gr::RenderDataID GetActiveCameraRenderDataID() const;
		re::BufferInput const& GetActiveCameraParams() const;

		void SetActiveCamera(gr::RenderDataID cameraRenderDataID, gr::TransformID cameraTransformID);


	public:
		bool ActiveAmbientLightHasChanged() const;
		bool HasActiveAmbientLight() const;
		gr::RenderDataID GetActiveAmbientLightID() const;

	
	public:
		void ShowImGuiWindow();


	private:
		void UpdateActiveAmbientLight();


	private:
		std::vector<std::unique_ptr<gr::GraphicsSystem>> m_graphicsSystems;
		std::map<std::string, size_t> m_scriptNameToIndex;

		gr::RenderDataManager const* m_renderData;

		gr::RenderDataID m_activeCameraRenderDataID;
		gr::TransformID m_activeCameraTransformDataID;
		re::BufferInput m_activeCameraParams;

		gr::RenderDataID m_activeAmbientLightRenderDataID;
		bool m_activeAmbientLightHasChanged;

		gr::RenderSystem* const m_owningRenderSystem;

		bool m_isCreated;


	private: // No copying allwoed
		GraphicsSystemManager(GraphicsSystemManager const&) = delete;
		GraphicsSystemManager(GraphicsSystemManager&&) noexcept = delete;
		GraphicsSystemManager& operator=(GraphicsSystemManager const&) = delete;
		GraphicsSystemManager& operator=(GraphicsSystemManager&&) noexcept = delete;

	};


	template<typename T>
	T* GraphicsSystemManager::GetGraphicsSystem() const
	{
		for (size_t i = 0; i < m_graphicsSystems.size(); i++)
		{
			T* gs = dynamic_cast<T*>(m_graphicsSystems[i].get());
			if (gs)
			{
				return gs;
			}
		}
		return nullptr;
	}


	inline gr::RenderDataManager const& GraphicsSystemManager::GetRenderData() const
	{
		return *m_renderData;
	}


	inline bool GraphicsSystemManager::ActiveAmbientLightHasChanged() const
	{
		return m_activeAmbientLightHasChanged;
	}


	inline bool GraphicsSystemManager::HasActiveAmbientLight() const
	{
		return m_activeAmbientLightRenderDataID != gr::k_invalidRenderDataID;
	}


	inline gr::RenderDataID GraphicsSystemManager::GetActiveAmbientLightID() const
	{
		return m_activeAmbientLightRenderDataID;
	}


	inline gr::RenderDataID GraphicsSystemManager::GetActiveCameraRenderDataID() const
	{
		return m_activeCameraRenderDataID;
	}
}

