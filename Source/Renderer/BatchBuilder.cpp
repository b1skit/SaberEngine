// © 2025 Adam Badke. All rights reserved.
#include "BatchBuilder.h"
#include "Buffer.h"
#include "Texture.h"


namespace re
{
	BatchBuilder::BatchBuilder(
		re::Lifetime lifetime,
		core::InvPtr<gr::MeshPrimitive> const& meshPrimitive,
		EffectID effectID)
		: m_batch(lifetime, meshPrimitive, effectID)
	{
	}


	BatchBuilder::BatchBuilder(
		re::Lifetime lifetime,
		gr::MeshPrimitive::RenderData const& meshPrimRenderData,
		gr::Material::MaterialInstanceRenderData const* materialInstanceData,
		re::Batch::VertexStreamOverride const* vertexStreamOverride)
		: m_batch(lifetime, meshPrimRenderData, materialInstanceData, vertexStreamOverride)
	{
	}


	BatchBuilder::BatchBuilder(
		re::Lifetime lifetime,
		re::Batch::RasterParams const& rasterParams,
		EffectID effectID,
		effect::drawstyle::Bitmask bitmask)
		: m_batch(lifetime, rasterParams, effectID, bitmask)
	{
	}


	BatchBuilder::BatchBuilder(
		re::Lifetime lifetime,
		re::Batch::ComputeParams const& computeParams,
		EffectID effectID)
		: m_batch(lifetime, computeParams, effectID)
	{
	}


	BatchBuilder::BatchBuilder(
		re::Lifetime lifetime,
		re::Batch::RayTracingParams const& rtParams)
		: m_batch(lifetime, rtParams)
	{
	}


	BatchBuilder::BatchBuilder(BatchBuilder&& rhs) noexcept
		: m_batch(std::move(rhs.m_batch))
	{
	}


	BatchBuilder& BatchBuilder::operator=(BatchBuilder&& rhs) noexcept
	{
		if (this != &rhs)
		{
			m_batch = std::move(rhs.m_batch);
		}
		return *this;
	}


	BatchBuilder& BatchBuilder::SetEffectID(EffectID effectID)
	{
		m_batch.SetEffectID(effectID);
		return *this;
	}


	BatchBuilder& BatchBuilder::SetBuffer(std::string const& shaderName, std::shared_ptr<re::Buffer> const& buffer)
	{
		m_batch.SetBuffer(shaderName, buffer);
		return *this;
	}


	BatchBuilder& BatchBuilder::SetBuffer(std::string const& shaderName, std::shared_ptr<re::Buffer> const& buffer, re::BufferView const& view)
	{
		m_batch.SetBuffer(shaderName, buffer, view);
		return *this;
	}


	BatchBuilder& BatchBuilder::SetBuffer(re::BufferInput&& bufferInput)
	{
		m_batch.SetBuffer(std::move(bufferInput));
		return *this;
	}


	BatchBuilder& BatchBuilder::SetBuffer(re::BufferInput const& bufferInput)
	{
		m_batch.SetBuffer(bufferInput);
		return *this;
	}


	BatchBuilder& BatchBuilder::AddTextureInput(
		char const* shaderName,
		core::InvPtr<re::Texture> const& texture,
		core::InvPtr<re::Sampler> const& sampler,
		re::TextureView const& texView)
	{
		m_batch.AddTextureInput(shaderName, texture, sampler, texView);
		return *this;
	}


	BatchBuilder& BatchBuilder::AddRWTextureInput(
		char const* shaderName,
		core::InvPtr<re::Texture> const& texture,
		re::TextureView const& texView)
	{
		m_batch.AddRWTextureInput(shaderName, texture, texView);
		return *this;
	}


	BatchBuilder& BatchBuilder::SetRootConstant(char const* shaderName, void const* src, re::DataType dataType)
	{
		m_batch.SetRootConstant(shaderName, src, dataType);
		return *this;
	}


	BatchBuilder& BatchBuilder::SetRootConstant(std::string const& shaderName, void const* src, re::DataType dataType)
	{
		m_batch.SetRootConstant(shaderName, src, dataType);
		return *this;
	}


	BatchBuilder& BatchBuilder::SetFilterMaskBit(re::Batch::Filter filterBit, bool enabled)
	{
		m_batch.SetFilterMaskBit(filterBit, enabled);
		return *this;
	}


	re::Batch BatchBuilder::Build() &&
	{
		return std::move(m_batch);
	}
}