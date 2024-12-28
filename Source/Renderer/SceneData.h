// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Texture.h"

#include "Core/InvPtr.h"

#include "Core/Util/HashUtils.h"
#include "Core/Util/StringHash.h"


namespace gr
{
	class Material;
	class MeshPrimitive;
	class VertexStream;
}

namespace re
{
	class Sampler;
	class Shader;
}

namespace re
{
	class SceneData final
	{
	public:
		explicit SceneData();
		SceneData(SceneData&&) noexcept = default;
		SceneData& operator=(SceneData&&) noexcept = default;
		
		~SceneData();

	public:
		void Initialize();
		void Destroy();

	public:		
		// Geometry:
		bool AddUniqueMeshPrimitive(std::shared_ptr<gr::MeshPrimitive>&); // Returns true if incoming ptr is modified

		void EndLoading();

		
	private:
		std::unordered_map<util::DataHash, std::shared_ptr<gr::MeshPrimitive>> m_meshPrimitives;
		mutable std::mutex m_meshPrimitivesMutex;

		bool m_isCreated; // Validate Destroy() was called after a scene was loaded


	private:
		SceneData(SceneData const&) = delete;
		SceneData& operator=(SceneData const&) = delete;
	};


	inline void SceneData::EndLoading()
	{
		m_isCreated = true;
	}
}