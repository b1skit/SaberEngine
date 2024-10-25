// © 2023 Adam Badke. All rights reserved.
#include "PipelineState.h"

#include "Core/Util/TextUtils.h"


namespace re
{
	PipelineState::PipelineState()
		: m_isDirty(true)
		, m_primitiveTopologyType(PrimitiveTopologyType::Triangle)

		// Rasterizer state. Note: Defaults as per D3D12: 
		// https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_rasterizer_desc#remarks
		, m_fillMode(FillMode::Solid)
		, m_faceCullingMode(FaceCullingMode::Back)
		, m_windingOrder(WindingOrder::CCW)
		, m_depthBias(0)
		, m_depthBiasClamp(0.f)
		, m_slopeScaledDepthBias(0.f)
		, m_depthClipEnable(true)
		, m_multisampleEnable(false)
		, m_antialiasedLineEnable(false)
		, m_forcedSampleCount(0)
		, m_conservativeRaster(false)
		
		// Depth stencil state: Note: Defaults as per D3D12:
		// https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_depth_stencil_desc#remarks
		, m_depthTestEnable(true)
		, m_depthWriteMask(DepthWriteMask::All)
		, m_depthFunc(ComparisonFunc::Less)
		, m_stencilEnabled(false)
		, m_stencilReadMask(k_defaultStencilReadMask)
		, m_stencilWriteMask(k_defaultStencilWriteMask)
		, m_frontFace{}
		, m_backFace{}

		// Blend state. Note: Defaults as per D3D12: 
		// https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_blend_desc#remarks
		, m_alphaToCoverageEnable(false)
		, m_independentBlendEnable(false)
		, m_renderTargetBlendDescs{}
	{
		ComputeDataHash();
	}


	void PipelineState::ComputeDataHash()
	{
		SEAssert(m_isDirty, "PipelineState data is not dirty");
		m_isDirty = false;

		ResetDataHash();

		AddDataBytesToHash(m_primitiveTopologyType);

		// Rasterizer state:
		AddDataBytesToHash(m_fillMode);
		AddDataBytesToHash(m_faceCullingMode);
		AddDataBytesToHash(m_windingOrder);
		AddDataBytesToHash(m_depthBias);
		AddDataBytesToHash(m_depthBiasClamp);
		AddDataBytesToHash(m_slopeScaledDepthBias);
		AddDataBytesToHash(m_depthClipEnable);
		AddDataBytesToHash(m_multisampleEnable);
		AddDataBytesToHash(m_antialiasedLineEnable);
		AddDataBytesToHash(m_forcedSampleCount);
		AddDataBytesToHash(m_conservativeRaster);

		// Depth stencil state:
		AddDataBytesToHash(m_depthTestEnable);
		AddDataBytesToHash(m_depthWriteMask);
		AddDataBytesToHash(m_depthFunc);
		AddDataBytesToHash(m_stencilEnabled);
		AddDataBytesToHash(m_stencilReadMask);
		AddDataBytesToHash(m_stencilWriteMask);
		AddDataBytesToHash(m_frontFace);
		AddDataBytesToHash(m_backFace);

		// Blend state:
		AddDataBytesToHash(m_alphaToCoverageEnable);
		AddDataBytesToHash(m_independentBlendEnable);
		for (auto const& renderTargetBlendDesc : m_renderTargetBlendDescs)
		{
			AddDataBytesToHash(renderTargetBlendDesc);
		}
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


	PipelineState::ComparisonFunc PipelineState::GetDepthComparison() const
	{
		SEAssert(!m_isDirty, "PipelineState is dirty");
		return m_depthFunc;
	}


	void PipelineState::SetDepthComparison(PipelineState::ComparisonFunc depthTestMode)
	{
		m_depthFunc = depthTestMode;
		m_isDirty = true;
		ComputeDataHash();
	}


	PipelineState::ComparisonFunc PipelineState::GetComparisonByName(char const* name)
	{
		static const std::map<std::string, PipelineState::ComparisonFunc> s_nameToType =
		{
			{"less", PipelineState::ComparisonFunc::Less},
			{"never", PipelineState::ComparisonFunc::Never},
			{"equal", PipelineState::ComparisonFunc::Equal},
			{"lequal", PipelineState::ComparisonFunc::LEqual},
			{"greater", PipelineState::ComparisonFunc::Greater},
			{"notequal", PipelineState::ComparisonFunc::NotEqual},
			{"gequal", PipelineState::ComparisonFunc::GEqual},
			{"always", PipelineState::ComparisonFunc::Always},
		};

		std::string const& lowerCaseName = util::ToLower(name);
		SEAssert(s_nameToType.contains(lowerCaseName), "Invalid type name string");

		return s_nameToType.at(lowerCaseName);
	}


	PipelineState::DepthWriteMask PipelineState::GetDepthWriteMaskByName(char const* name)
	{
		static const std::map<std::string, PipelineState::DepthWriteMask> s_nameToType =
		{
			{"zero", PipelineState::DepthWriteMask::Zero},
			{"all", PipelineState::DepthWriteMask::All},
		};

		std::string const& lowerCaseName = util::ToLower(name);
		SEAssert(s_nameToType.contains(lowerCaseName), "Invalid type name string");

		return s_nameToType.at(lowerCaseName);
	}


	PipelineState::StencilOp PipelineState::GetStencilOpByName(char const* name)
	{
		static const std::map<std::string, PipelineState::StencilOp> s_nameToType =
		{
			{"keep", PipelineState::StencilOp::Keep},
			{"zero", PipelineState::StencilOp::Zero},
			{"replace", PipelineState::StencilOp::Replace},
			{"incrementsaturate", PipelineState::StencilOp::IncrementSaturate},
			{"decrementsaturate", PipelineState::StencilOp::DecrementSaturate},
			{"invert", PipelineState::StencilOp::Invert},
			{"increment", PipelineState::StencilOp::Increment},
			{"decrement", PipelineState::StencilOp::Decrement},
		};

		std::string const& lowerCaseName = util::ToLower(name);
		SEAssert(s_nameToType.contains(lowerCaseName), "Invalid type name string");

		return s_nameToType.at(lowerCaseName);
	}


	PipelineState::BlendMode PipelineState::GetBlendModeByName(char const* name)
	{
		static const std::map<std::string, PipelineState::BlendMode> s_nameToType =
		{
			{"zero", PipelineState::BlendMode::Zero},
			{"one", PipelineState::BlendMode::One},
			{"srccolor", PipelineState::BlendMode::SrcColor},
			{"invsrccolor", PipelineState::BlendMode::InvSrcColor},
			{"srcalpha", PipelineState::BlendMode::SrcAlpha},
			{"invsrcalpha", PipelineState::BlendMode::InvSrcAlpha},
			{"dstalpha", PipelineState::BlendMode::DstAlpha},
			{"invdstalpha", PipelineState::BlendMode::InvDstAlpha},
			{"dstcolor", PipelineState::BlendMode::DstColor},
			{"invdstcolor", PipelineState::BlendMode::InvDstColor},
			{"srcalphasat", PipelineState::BlendMode::SrcAlphaSat},
			{"blendfactor", PipelineState::BlendMode::BlendFactor},
			{"invblendfactor", PipelineState::BlendMode::InvBlendFactor},
			{"srconecolor", PipelineState::BlendMode::SrcOneColor},
			{"invsrconecolor", PipelineState::BlendMode::InvSrcOneColor},
			{"srconealpha", PipelineState::BlendMode::SrcOneAlpha},
			{"invsrconealpha", PipelineState::BlendMode::InvSrcOneAlpha},
			{"alphafactor", PipelineState::BlendMode::AlphaFactor},
			{"invalphafactor", PipelineState::BlendMode::InvAlphaFactor},
		};

		std::string const& lowerCaseName = util::ToLower(name);
		SEAssert(s_nameToType.contains(lowerCaseName), "Invalid type name string");

		return s_nameToType.at(lowerCaseName);
	}


	PipelineState::BlendOp PipelineState::GetBlendOpByName(char const* name)
	{
		static const std::map<std::string, PipelineState::BlendOp> s_nameToType =
		{
			{"add", PipelineState::BlendOp::Add},
			{"subtract", PipelineState::BlendOp::Subtract},
			{"revsubtract", PipelineState::BlendOp::RevSubtract},
			{"min", PipelineState::BlendOp::Min},
			{"max", PipelineState::BlendOp::Max},
		};

		std::string const& lowerCaseName = util::ToLower(name);
		SEAssert(s_nameToType.contains(lowerCaseName), "Invalid type name string");

		return s_nameToType.at(lowerCaseName);
	}


	PipelineState::LogicOp PipelineState::GetLogicOpByName(char const* name)
	{
		static const std::map<std::string, PipelineState::LogicOp> s_nameToType =
		{
			{"clear", PipelineState::LogicOp::Clear},
			{"set", PipelineState::LogicOp::Set},
			{"copy", PipelineState::LogicOp::Copy},
			{"copyinverted", PipelineState::LogicOp::CopyInverted},
			{"noop", PipelineState::LogicOp::NoOp},
			{"invert", PipelineState::LogicOp::Invert},
			{"and", PipelineState::LogicOp::AND},
			{"nand", PipelineState::LogicOp::NAND},
			{"or", PipelineState::LogicOp::OR},
			{"nor", PipelineState::LogicOp::NOR},
			{"xor", PipelineState::LogicOp::XOR},
			{"equiv", PipelineState::LogicOp::EQUIV},
			{"andreverse", PipelineState::LogicOp::ANDReverse},
			{"andinverted", PipelineState::LogicOp::AndInverted},
			{"orreverse", PipelineState::LogicOp::ORReverse},
			{"orinverted", PipelineState::LogicOp::ORInverted},
		};

		std::string const& lowerCaseName = util::ToLower(name);
		SEAssert(s_nameToType.contains(lowerCaseName), "Invalid type name string");

		return s_nameToType.at(lowerCaseName);
	}
}