// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "BufferView.h"
#include "GraphicsEvent.h"
#include "RenderDataManager.h"
#include "RenderObjectIDs.h"


namespace gr
{
	class GraphicsSystem;
	class RenderSystem;

	template<typename T>
	concept GraphicsSystemType = std::derived_from<T, gr::GraphicsSystem>;


	class GraphicsSystemManager
	{
	public:
		GraphicsSystemManager(gr::RenderSystem*, uint8_t numFramesInFlight);
		~GraphicsSystemManager() = default;

		void Destroy();

		void Create();
		void PreRender(uint64_t currentFrameNum);

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

		uint64_t GetCurrentRenderFrameNum() const;
		uint8_t GetNumFramesInFlight() const;


	public:
		gr::RenderDataID GetActiveCameraRenderDataID() const;
		re::BufferInput const& GetActiveCameraParams() const;

		void SetActiveCamera(gr::RenderDataID cameraRenderDataID, gr::TransformID cameraTransformID);


	public:
		bool ActiveAmbientLightHasChanged() const;
		bool HasActiveAmbientLight() const;
		gr::RenderDataID GetActiveAmbientLightID() const;


	public: // Graphics system events: These functions are only available to GraphicsSystems
		template<typename T>
		void SubscribeToGraphicsEvent(
			util::CHashKey const& eventKey, gr::GraphicsSystem* listener) requires GraphicsSystemType<T>;

		template<typename T>
		void PostGraphicsEvent(gr::GraphicsEvent const& newEvent) requires GraphicsSystemType<T>;

		template<typename T>
		void PostGraphicsEvent(util::CHashKey const&, GraphicsEvent::GraphicsEventData&&) requires GraphicsSystemType<T>;


	private:
		std::unordered_map<util::CHashKey, std::vector<gr::GraphicsSystem*>> m_eventListeners;


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

		gr::RenderSystem const* m_owningRenderSystem;

		uint64_t m_currentFrameNum;
		uint8_t m_numFramesInFlight;
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


	inline uint64_t GraphicsSystemManager::GetCurrentRenderFrameNum() const
	{
		return m_currentFrameNum;
	}


	inline uint8_t GraphicsSystemManager::GetNumFramesInFlight() const
	{
		return m_numFramesInFlight;
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


	template<typename T>
	void GraphicsSystemManager::SubscribeToGraphicsEvent(
		util::CHashKey const& eventKey, gr::GraphicsSystem* listener)
		requires GraphicsSystemType<T>
	{
		m_eventListeners[eventKey].emplace_back(listener);
	}


	template<typename T>
	void GraphicsSystemManager::PostGraphicsEvent(gr::GraphicsEvent const& newEvent)
		requires GraphicsSystemType<T>
	{
		auto itr = m_eventListeners.find(newEvent.m_eventKey);
		if (itr != m_eventListeners.end())
		{
			for (auto& listener : itr->second)
			{
				listener->PostEvent(gr::GraphicsEvent(newEvent));
			}
		}
	}


	template<typename T>
	void GraphicsSystemManager::PostGraphicsEvent(util::CHashKey const& eventKey, GraphicsEvent::GraphicsEventData&& data)
		requires GraphicsSystemType<T>
	{
		PostGraphicsEvent<T>(gr::GraphicsEvent{
			.m_eventKey = std::move(eventKey),
			.m_data = std::forward<GraphicsEvent::GraphicsEventData>(data)
			});
	}
}

