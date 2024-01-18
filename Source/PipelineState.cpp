// © 2023 Adam Badke. All rights reserved.
#include "PipelineState.h"


namespace re
{
	PipelineState::PipelineState()
		: m_isDirty(true)
		, m_topologyType(TopologyType::Triangle)
		, m_fillMode(FillMode::Solid)
		, m_faceCullingMode(FaceCullingMode::Back)
		, m_windingOrder(WindingOrder::CCW)
		, m_depthTestMode(DepthTestMode::Less)
	{
		ComputeDataHash();
	}


	void PipelineState::ComputeDataHash()
	{
		SEAssert(m_isDirty, "PipelineState data is not dirty");
		m_isDirty = false;

		ResetDataHash();

		AddDataBytesToHash(m_topologyType);
		AddDataBytesToHash(m_fillMode);
		AddDataBytesToHash(m_faceCullingMode);
		AddDataBytesToHash(m_windingOrder);
		AddDataBytesToHash(m_depthTestMode);
	}


	uint64_t PipelineState::GetPipelineStateDataHash() const
	{
		SEAssert(!m_isDirty, "Trying to get the data hash from a dirty pipeline state");
		return GetDataHash();
	}


	PipelineState::TopologyType PipelineState::GetTopologyType() const
	{
		SEAssert(!m_isDirty, "PipelineState is dirty");
		return m_topologyType;
	}


	void PipelineState::SetTopologyType(PipelineState::TopologyType topologyType)
	{
		m_topologyType = topologyType;
		m_isDirty = true;
		ComputeDataHash();
	}


	PipelineState::FillMode PipelineState::GetFillMode() const
	{
		SEAssert(!m_isDirty, "PipelineState is dirty");
		return m_fillMode;
	}

	void PipelineState::SetFillMode(FillMode fillMode)
	{
		m_fillMode = fillMode;
		m_isDirty = true;
		ComputeDataHash();
	}


	PipelineState::FaceCullingMode PipelineState::GetFaceCullingMode() const
	{
		SEAssert(!m_isDirty, "PipelineState is dirty");
		return m_faceCullingMode;
	}


	void PipelineState::SetFaceCullingMode(PipelineState::FaceCullingMode faceCullingMode)
	{
		m_faceCullingMode = faceCullingMode;
		m_isDirty = true;
		ComputeDataHash();
	}


	PipelineState::WindingOrder PipelineState::GetWindingOrder() const
	{
		SEAssert(!m_isDirty, "PipelineState is dirty");
		return m_windingOrder;
	}


	void PipelineState::SetWindingOrder(PipelineState::WindingOrder windingOrder)
	{
		m_windingOrder = windingOrder;
		m_isDirty = true;
		ComputeDataHash();
	}


	PipelineState::DepthTestMode PipelineState::GetDepthTestMode() const
	{
		SEAssert(!m_isDirty, "PipelineState is dirty");
		return m_depthTestMode;
	}


	void PipelineState::SetDepthTestMode(PipelineState::DepthTestMode depthTestMode)
	{
		m_depthTestMode = depthTestMode;
		m_isDirty = true;
		ComputeDataHash();
	}
}