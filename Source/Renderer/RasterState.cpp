// © 2023 Adam Badke. All rights reserved.
#include "RasterState.h"

#include "Core/Util/CHashKey.h"
#include "Core/Util/TextUtils.h"


namespace re
{
	RasterState::RasterState()
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


	void RasterState::ComputeDataHash()
	{
		SEAssert(m_isDirty, "RasterState data is not dirty");
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


	RasterState::PrimitiveTopologyType RasterState::CStrToPrimitiveTopologyType(char const* name)
	{
		util::CHashKey const& nameHash = util::CHashKey::Create(util::ToLower(name));
		switch (nameHash)
		{
		case util::CHashKey("triangle"): return RasterState::PrimitiveTopologyType::Triangle;
		case util::CHashKey("point"): return RasterState::PrimitiveTopologyType::Point;
		case util::CHashKey("line"): return RasterState::PrimitiveTopologyType::Line;
		case util::CHashKey("patch"): return RasterState::PrimitiveTopologyType::Patch;
		default: SEAssertF("Invalid type name string");
		}
		return RasterState::PrimitiveTopologyType::Triangle; // This should never happen
	}


	RasterState::FillMode RasterState::GetFillModeByName(char const* name)
	{
		util::CHashKey const& nameHash = util::CHashKey::Create(util::ToLower(name));
		switch (nameHash)
		{
		case util::CHashKey("solid"): return RasterState::FillMode::Solid;
		case util::CHashKey("wireframe"): return RasterState::FillMode::Wireframe;
		default: SEAssertF("Invalid type name string");
		}
		return RasterState::FillMode::Solid; // This should never happen
	}


	RasterState::FaceCullingMode RasterState::GetFaceCullingModeByName(char const* name)
	{
		util::CHashKey const& nameHash = util::CHashKey::Create(util::ToLower(name));
		switch (nameHash)
		{
		case util::CHashKey("back"): return RasterState::FaceCullingMode::Back;
		case util::CHashKey("front"): return RasterState::FaceCullingMode::Front;
		case util::CHashKey("disabled"): return RasterState::FaceCullingMode::Disabled;
		default: SEAssertF("Invalid type name string");
		}
		return RasterState::FaceCullingMode::Back; // This should never happen
	}


	RasterState::WindingOrder RasterState::GetWindingOrderByName(char const* name)
	{
		util::CHashKey const& nameHash = util::CHashKey::Create(util::ToLower(name));
		switch (nameHash)
		{
		case util::CHashKey("ccw"): return RasterState::WindingOrder::CCW;
		case util::CHashKey("cw"): return RasterState::WindingOrder::CW;
		default: SEAssertF("Invalid type name string");
		}
		return RasterState::WindingOrder::CCW; // This should never happen
	}


	RasterState::ComparisonFunc RasterState::GetComparisonByName(char const* name)
	{
		util::CHashKey const& nameHash = util::CHashKey::Create(util::ToLower(name));
		switch (nameHash)
		{
		case util::CHashKey("less"): return RasterState::ComparisonFunc::Less;
		case util::CHashKey("never"): return RasterState::ComparisonFunc::Never;
		case util::CHashKey("equal"): return RasterState::ComparisonFunc::Equal;
		case util::CHashKey("lequal"): return RasterState::ComparisonFunc::LEqual;
		case util::CHashKey("greater"): return RasterState::ComparisonFunc::Greater;
		case util::CHashKey("notequal"): return RasterState::ComparisonFunc::NotEqual;
		case util::CHashKey("gequal"): return RasterState::ComparisonFunc::GEqual;
		case util::CHashKey("always"): return RasterState::ComparisonFunc::Always;
		default: SEAssertF("Invalid type name string");
		}
		return RasterState::ComparisonFunc::Less; // This should never happen
	}


	RasterState::DepthWriteMask RasterState::GetDepthWriteMaskByName(char const* name)
	{
		util::CHashKey const& nameHash = util::CHashKey::Create(util::ToLower(name));
		switch (nameHash)
		{
		case util::CHashKey("zero"): return RasterState::DepthWriteMask::Zero;
		case util::CHashKey("all"): return RasterState::DepthWriteMask::All;
		default: SEAssertF("Invalid type name string");
		}
		return RasterState::DepthWriteMask::All; // This should never happen
	}


	RasterState::StencilOp RasterState::GetStencilOpByName(char const* name)
	{
		util::CHashKey const& nameHash = util::CHashKey::Create(util::ToLower(name));
		switch (nameHash)
		{
		case util::CHashKey("keep"): return RasterState::StencilOp::Keep;
		case util::CHashKey("zero"): return RasterState::StencilOp::Zero;
		case util::CHashKey("replace"): return RasterState::StencilOp::Replace;
		case util::CHashKey("incrementsaturate"): return RasterState::StencilOp::IncrementSaturate;
		case util::CHashKey("decrementsaturate"): return RasterState::StencilOp::DecrementSaturate;
		case util::CHashKey("invert"): return RasterState::StencilOp::Invert;
		case util::CHashKey("increment"): return RasterState::StencilOp::Increment;
		case util::CHashKey("decrement"): return RasterState::StencilOp::Decrement;
		default: SEAssertF("Invalid type name string");
		}
		return RasterState::StencilOp::Keep; // This should never happen
	}


	RasterState::BlendMode RasterState::GetBlendModeByName(char const* name)
	{
		util::CHashKey const& nameHash = util::CHashKey::Create(util::ToLower(name));
		switch (nameHash)
		{
		case util::CHashKey("zero"): return RasterState::BlendMode::Zero;
		case util::CHashKey("one"): return RasterState::BlendMode::One;
		case util::CHashKey("srccolor"): return RasterState::BlendMode::SrcColor;
		case util::CHashKey("invsrccolor"): return RasterState::BlendMode::InvSrcColor;
		case util::CHashKey("srcalpha"): return RasterState::BlendMode::SrcAlpha;
		case util::CHashKey("invsrcalpha"): return RasterState::BlendMode::InvSrcAlpha;
		case util::CHashKey("dstalpha"): return RasterState::BlendMode::DstAlpha;
		case util::CHashKey("invdstalpha"): return RasterState::BlendMode::InvDstAlpha;
		case util::CHashKey("dstcolor"): return RasterState::BlendMode::DstColor;
		case util::CHashKey("invdstcolor"): return RasterState::BlendMode::InvDstColor;
		case util::CHashKey("srcalphasat"): return RasterState::BlendMode::SrcAlphaSat;
		case util::CHashKey("blendfactor"): return RasterState::BlendMode::BlendFactor;
		case util::CHashKey("invblendfactor"): return RasterState::BlendMode::InvBlendFactor;
		case util::CHashKey("srconecolor"): return RasterState::BlendMode::SrcOneColor;
		case util::CHashKey("invsrconecolor"): return RasterState::BlendMode::InvSrcOneColor;
		case util::CHashKey("srconealpha"): return RasterState::BlendMode::SrcOneAlpha;
		case util::CHashKey("invsrconealpha"): return RasterState::BlendMode::InvSrcOneAlpha;
		case util::CHashKey("alphafactor"): return RasterState::BlendMode::AlphaFactor;
		case util::CHashKey("invalphafactor"): return RasterState::BlendMode::InvAlphaFactor;
		default: SEAssertF("Invalid type name string");
		};
		return RasterState::BlendMode::Zero; // This should never happen
	}


	RasterState::BlendOp RasterState::GetBlendOpByName(char const* name)
	{
		util::CHashKey const& nameHash = util::CHashKey::Create(util::ToLower(name));
		switch (nameHash)
		{
			case util::CHashKey("add"): return RasterState::BlendOp::Add;
			case util::CHashKey("subtract"): return RasterState::BlendOp::Subtract;
			case util::CHashKey("revsubtract"): return RasterState::BlendOp::RevSubtract;
			case util::CHashKey("min"): return RasterState::BlendOp::Min;
			case util::CHashKey("max"): return RasterState::BlendOp::Max;
			default: SEAssertF("Invalid type name string");
		};
		return RasterState::BlendOp::Add; // This should never happen
	}


	RasterState::LogicOp RasterState::GetLogicOpByName(char const* name)
	{
		util::CHashKey const& nameHash = util::CHashKey::Create(util::ToLower(name));
		switch (nameHash)
		{
			case util::CHashKey("clear"): return RasterState::LogicOp::Clear;
			case util::CHashKey("set"): return RasterState::LogicOp::Set;
			case util::CHashKey("copy"): return RasterState::LogicOp::Copy;
			case util::CHashKey("copyinverted"): return RasterState::LogicOp::CopyInverted;
			case util::CHashKey("noop"): return RasterState::LogicOp::NoOp;
			case util::CHashKey("invert"): return RasterState::LogicOp::Invert;
			case util::CHashKey("and"): return RasterState::LogicOp::AND;
			case util::CHashKey("nand"): return RasterState::LogicOp::NAND;
			case util::CHashKey("or"): return RasterState::LogicOp::OR;
			case util::CHashKey("nor"): return RasterState::LogicOp::NOR;
			case util::CHashKey("xor"): return RasterState::LogicOp::XOR;
			case util::CHashKey("equiv"): return RasterState::LogicOp::EQUIV;
			case util::CHashKey("andreverse"): return RasterState::LogicOp::ANDReverse;
			case util::CHashKey("andinverted"): return RasterState::LogicOp::AndInverted;
			case util::CHashKey("orreverse"): return RasterState::LogicOp::ORReverse;
			case util::CHashKey("orinverted"): return RasterState::LogicOp::ORInverted;
			default: SEAssertF("Invalid type name string");
		};
		return RasterState::LogicOp::Clear; // This should never happen
	}
}