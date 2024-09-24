// © 2023 Adam Badke. All rights reserved.
#include "PipelineState.h"

#include "Core/Util/TextUtils.h"


namespace re
{
	PipelineState::PipelineState()
		: m_isDirty(true)
		, m_primitiveTopologyType(PrimitiveTopologyType::Triangle)
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

		AddDataBytesToHash(m_primitiveTopologyType);
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


	PipelineState::PrimitiveTopologyType PipelineState::GetPrimitiveTopologyType() const
	{
		SEAssert(!m_isDirty, "PipelineState is dirty");
		return m_primitiveTopologyType;
	}


	void PipelineState::SetPrimitiveTopologyType(PipelineState::PrimitiveTopologyType topologyType)
	{
		m_primitiveTopologyType = topologyType;
		m_isDirty = true;
		ComputeDataHash();
	}


	PipelineState::PrimitiveTopologyType PipelineState::CStrToPrimitiveTopologyType(char const* name)
	{
		static const std::map<std::string, PipelineState::PrimitiveTopologyType> s_nameToType =
		{
			{"triangle", PipelineState::PrimitiveTopologyType::Triangle},
			{"point", PipelineState::PrimitiveTopologyType::Point},
			{"line", PipelineState::PrimitiveTopologyType::Line},
			{"patch", PipelineState::PrimitiveTopologyType::Patch},
		};
		
		std::string const& lowerCaseName = util::ToLower(name);
		SEAssert(s_nameToType.contains(lowerCaseName), "Invalid type name string");

		return s_nameToType.at(lowerCaseName);
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


	PipelineState::FillMode PipelineState::GetFillModeByName(char const* name)
	{
		static const std::map<std::string, PipelineState::FillMode> s_nameToType =
		{
			{"solid", PipelineState::FillMode::Solid},
			{"wireframe", PipelineState::FillMode::Wireframe},
		};

		std::string const& lowerCaseName = util::ToLower(name);
		SEAssert(s_nameToType.contains(lowerCaseName), "Invalid type name string");

		return s_nameToType.at(lowerCaseName);
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


	PipelineState::FaceCullingMode PipelineState::GetFaceCullingModeByName(char const* name)
	{
		static const std::map<std::string, PipelineState::FaceCullingMode> s_nameToType =
		{
			{"back", PipelineState::FaceCullingMode::Back},
			{"front", PipelineState::FaceCullingMode::Front},
			{"disabled", PipelineState::FaceCullingMode::Disabled},
		};

		std::string const& lowerCaseName = util::ToLower(name);
		SEAssert(s_nameToType.contains(lowerCaseName), "Invalid type name string");

		return s_nameToType.at(lowerCaseName);
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


	PipelineState::WindingOrder PipelineState::GetWindingOrderByName(char const* name)
	{
		static const std::map<std::string, PipelineState::WindingOrder> s_nameToType =
		{
			{"ccw", PipelineState::WindingOrder::CCW},
			{"cw", PipelineState::WindingOrder::CW},
		};

		std::string const& lowerCaseName = util::ToLower(name);
		SEAssert(s_nameToType.contains(lowerCaseName), "Invalid type name string");

		return s_nameToType.at(lowerCaseName);
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


	PipelineState::DepthTestMode PipelineState::GetDepthTestModeByName(char const* name)
	{
		static const std::map<std::string, PipelineState::DepthTestMode> s_nameToType =
		{
			{"less", PipelineState::DepthTestMode::Less},
			{"never", PipelineState::DepthTestMode::Never},
			{"equal", PipelineState::DepthTestMode::Equal},
			{"lequal", PipelineState::DepthTestMode::LEqual},
			{"greater", PipelineState::DepthTestMode::Greater},
			{"notequal", PipelineState::DepthTestMode::NotEqual},
			{"gequal", PipelineState::DepthTestMode::GEqual},
			{"always", PipelineState::DepthTestMode::Always},
		};

		std::string const& lowerCaseName = util::ToLower(name);
		SEAssert(s_nameToType.contains(lowerCaseName), "Invalid type name string");

		return s_nameToType.at(lowerCaseName);
	}
}