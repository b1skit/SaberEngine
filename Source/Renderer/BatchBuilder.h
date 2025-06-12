// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "Batch.h"
#include "BufferView.h"
#include "Effect.h"
#include "Material.h"
#include "MeshPrimitive.h"
#include "RootConstants.h"
#include "Sampler.h"
#include "TextureView.h"

#include "Core/InvPtr.h"


namespace re
{
	class Buffer;
	class Texture;
}


namespace re
{
	class BatchBuilder final
	{
	public:
		// Raster batch builders:
		BatchBuilder(re::Lifetime, core::InvPtr<gr::MeshPrimitive> const&, EffectID);
		
		BatchBuilder(re::Lifetime,
			gr::MeshPrimitive::RenderData const&,
			gr::Material::MaterialInstanceRenderData const*,
			re::Batch::VertexStreamOverride const* = nullptr);
		
		BatchBuilder(re::Lifetime, re::Batch::RasterParams const&, EffectID, effect::drawstyle::Bitmask);

		// Compute batch builder:
		BatchBuilder(re::Lifetime, re::Batch::ComputeParams const&, EffectID);

		// Ray tracing batch builder:
		BatchBuilder(re::Lifetime, re::Batch::RayTracingParams const&);


	public:
		BatchBuilder(BatchBuilder&&) noexcept;
		BatchBuilder& operator=(BatchBuilder&&) noexcept;
		~BatchBuilder() = default;


	public:
		BatchBuilder& SetEffectID(EffectID);

		BatchBuilder& SetBuffer(std::string const& shaderName, std::shared_ptr<re::Buffer> const&);
		BatchBuilder& SetBuffer(std::string const& shaderName, std::shared_ptr<re::Buffer> const&, re::BufferView const&);
		BatchBuilder& SetBuffer(re::BufferInput&&);
		BatchBuilder& SetBuffer(re::BufferInput const&);

		BatchBuilder& AddTextureInput(
			char const* shaderName,
			core::InvPtr<re::Texture> const&,
			core::InvPtr<re::Sampler> const&,
			re::TextureView const&);

		BatchBuilder& AddRWTextureInput(
			char const* shaderName,
			core::InvPtr<re::Texture> const&,
			re::TextureView const&);

		BatchBuilder& SetRootConstant(char const* shaderName, void const* src, re::DataType);
		BatchBuilder& SetRootConstant(std::string const& shaderName, void const* src, re::DataType);

		BatchBuilder& SetFilterMaskBit(re::Batch::Filter filterBit, bool enabled);

		re::Batch Build() &&;


	private:
		re::Batch m_batch;


	private:
		BatchBuilder(BatchBuilder const&) = delete;
		BatchBuilder& operator=(BatchBuilder const&) = delete;
		BatchBuilder() = delete;
	};
}