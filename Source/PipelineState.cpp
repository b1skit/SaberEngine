// © 2023 Adam Badke. All rights reserved.
#include "PipelineState.h"


namespace re
{
	PipelineState::PipelineState()
		: m_isDirty(true)
		, m_fillMode(FillMode::Solid)
		, m_faceCullingMode(FaceCullingMode::Back)
		, m_windingOrder(WindingOrder::CCW)
		, m_depthTestMode(DepthTestMode::Default)
		, m_targetClearMode(ClearTarget::None)
	{
		ComputeDataHash();
	}


	void PipelineState::ComputeDataHash()
	{
		SEAssert("PipelineState data is not dirty", m_isDirty);
		m_isDirty = false;

		ResetDataHash();

		AddDataBytesToHash(m_fillMode);
		AddDataBytesToHash(m_faceCullingMode);
		AddDataBytesToHash(m_windingOrder);
		AddDataBytesToHash(m_depthTestMode);
		AddDataBytesToHash(m_targetClearMode);		
	}


	uint64_t PipelineState::GetPipelineStateDataHash() const
	{
		SEAssert("Trying to get the data hash from a dirty pipeline state", !m_isDirty);
		return GetDataHash();
	}


	PipelineState::FillMode PipelineState::GetFillMode() const
	{
		SEAssert("PipelineState is dirty", !m_isDirty);
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
		SEAssert("PipelineState is dirty", !m_isDirty);
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
		SEAssert("PipelineState is dirty", !m_isDirty);
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
		SEAssert("PipelineState is dirty", !m_isDirty);
		return m_depthTestMode;
	}


	void PipelineState::SetDepthTestMode(PipelineState::DepthTestMode depthTestMode)
	{
		m_depthTestMode = depthTestMode;
		m_isDirty = true;
		ComputeDataHash();
	}


	PipelineState::ClearTarget PipelineState::GetClearTarget() const
	{
		SEAssert("PipelineState is dirty", !m_isDirty);
		return m_targetClearMode;
	}


	void PipelineState::SetClearTarget(PipelineState::ClearTarget targetClearMode)
	{
		m_targetClearMode = targetClearMode;
		m_isDirty = true;
		ComputeDataHash();
	}
}