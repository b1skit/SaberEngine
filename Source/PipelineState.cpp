// © 2023 Adam Badke. All rights reserved.
#include "PipelineState.h"


namespace gr
{
	PipelineState::PipelineState()
		: m_isDirty(true)
		, m_fillMode(FillMode::Solid)
		, m_faceCullingMode(FaceCullingMode::Back)
		, m_windingOrder(WindingOrder::CCW)
		, m_depthTestMode(DepthTestMode::Default)
		, m_depthWriteMode(DepthWriteMode::Enabled)
		, m_targetClearMode(ClearTarget::None)
	{
	}


	void PipelineState::ComputeDataHash()
	{
		SEAssert("PipelineState data is not dirty", m_isDirty);

		AddDataBytesToHash(m_fillMode);
		AddDataBytesToHash(m_faceCullingMode);
		AddDataBytesToHash(m_windingOrder);
		AddDataBytesToHash(m_depthTestMode);
		AddDataBytesToHash(m_depthWriteMode);
		AddDataBytesToHash(m_targetClearMode);
	}


	uint64_t PipelineState::GetPipelineStateDataHash()
	{
		if (m_isDirty)
		{
			ComputeDataHash();
			m_isDirty = false;
		}
		return GetDataHash();
	}


	PipelineState::FillMode PipelineState::GetFillMode() const
	{
		return m_fillMode;
	}

	void PipelineState::SetFillMode(FillMode fillMode)
	{
		m_fillMode = fillMode;
		m_isDirty = true;
	}


	PipelineState::FaceCullingMode PipelineState::GetFaceCullingMode() const
	{
		return m_faceCullingMode;
	}


	void PipelineState::SetFaceCullingMode(PipelineState::FaceCullingMode faceCullingMode)
	{
		m_faceCullingMode = faceCullingMode;
		m_isDirty = true;
	}


	PipelineState::WindingOrder PipelineState::GetWindingOrder() const
	{
		return m_windingOrder;
	}


	void PipelineState::SetWindingOrder(PipelineState::WindingOrder windingOrder)
	{
		m_windingOrder = windingOrder;
		m_isDirty = true;
	}


	PipelineState::DepthTestMode PipelineState::GetDepthTestMode() const
	{
		return m_depthTestMode;
	}


	void PipelineState::SetDepthTestMode(PipelineState::DepthTestMode depthTestMode)
	{
		m_depthTestMode = depthTestMode;
		m_isDirty = true;
	}


	PipelineState::DepthWriteMode PipelineState::GetDepthWriteMode() const
	{
		return m_depthWriteMode;
	}


	void PipelineState::SetDepthWriteMode(PipelineState::DepthWriteMode depthWriteMode)
	{
		m_depthWriteMode = depthWriteMode;
		m_isDirty = true;
	}


	PipelineState::ClearTarget PipelineState::GetClearTarget() const
	{
		return m_targetClearMode;
	}


	void PipelineState::SetClearTarget(PipelineState::ClearTarget targetClearMode)
	{
		m_targetClearMode = targetClearMode;
		m_isDirty = true;
	}
}