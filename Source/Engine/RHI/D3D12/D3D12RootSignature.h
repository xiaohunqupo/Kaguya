#pragma once
#include <vector>
#include <bitset>
#include "D3D12Types.h"

namespace RHI
{
	// The maximum size of a root signature is 64 DWORDs.
	// This maximum size is chosen to prevent abuse of the root signature as a way of storing bulk data. Each entry in the
	// root signature has a cost towards this 64 DWORD limit:
	// Descriptor tables cost 1 DWORD each.
	// Root constants cost 1 DWORD each, since they are 32-bit values.
	// Root descriptors (64-bit GPU virtual addresses) cost 2 DWORDs each.
	// Static samplers do not have any cost in the size of the root signature.
	// Use a small a root signature as necessary, though balance this with the flexibility of a larger root signature.
	// Arrange parameters in a large root signature so that the parameters most likely to change often, or if low access
	// latency for a given parameter is important, occur first. If convenient, use root constants or root constant buffer
	// views over putting constant buffer views in a descriptor heap.

	// Describes ranges of descriptors that might have offsets relative to the initial range descriptor
	// I use this to emulate bindless resource, SM6.6 provides global variables for DescriptorHeaps
	// that you can directly index into
	class D3D12DescriptorTable
	{
	public:
		explicit D3D12DescriptorTable(size_t NumRanges);

		D3D12DescriptorTable& AddSRVRange(
			UINT						 BaseShaderRegister,
			UINT						 RegisterSpace,
			UINT						 NumDescriptors,
			D3D12_DESCRIPTOR_RANGE_FLAGS Flags,
			UINT						 OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);

		D3D12DescriptorTable& AddUAVRange(
			UINT						 BaseShaderRegister,
			UINT						 RegisterSpace,
			UINT						 NumDescriptors,
			D3D12_DESCRIPTOR_RANGE_FLAGS Flags,
			UINT						 OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);

		D3D12DescriptorTable& AddCBVRange(
			UINT						 BaseShaderRegister,
			UINT						 RegisterSpace,
			UINT						 NumDescriptors,
			D3D12_DESCRIPTOR_RANGE_FLAGS Flags,
			UINT						 OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);

		D3D12DescriptorTable& AddSamplerRange(
			UINT						 BaseShaderRegister,
			UINT						 RegisterSpace,
			UINT						 NumDescriptors,
			D3D12_DESCRIPTOR_RANGE_FLAGS Flags,
			UINT						 OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);

		D3D12DescriptorTable& AddDescriptorRange(
			UINT						 BaseShaderRegister,
			UINT						 RegisterSpace,
			D3D12_DESCRIPTOR_RANGE_TYPE	 Type,
			UINT						 NumDescriptors,
			D3D12_DESCRIPTOR_RANGE_FLAGS Flags,
			UINT						 OffsetInDescriptorsFromTableStart);

		UINT						   size() const noexcept;
		const D3D12_DESCRIPTOR_RANGE1* data() const noexcept;

	private:
		std::vector<CD3DX12_DESCRIPTOR_RANGE1> DescriptorRanges;
	};

	class RootSignatureDesc
	{
	public:
		RootSignatureDesc();

		RootSignatureDesc& AddDescriptorTable(const D3D12DescriptorTable& DescriptorTable);

		template<typename T>
		RootSignatureDesc& Add32BitConstants(UINT ShaderRegister, UINT RegisterSpace)
		{
			static_assert(sizeof(T) % 4 == 0);
			CD3DX12_ROOT_PARAMETER1 Parameter = {};
			Parameter.InitAsConstants(sizeof(T) / 4, ShaderRegister, RegisterSpace);
			return AddParameter(Parameter);
		}

		RootSignatureDesc& Add32BitConstants(UINT ShaderRegister, UINT RegisterSpace, UINT Num32BitValues)
		{
			CD3DX12_ROOT_PARAMETER1 Parameter = {};
			Parameter.InitAsConstants(Num32BitValues, ShaderRegister, RegisterSpace);
			return AddParameter(Parameter);
		}

		RootSignatureDesc& AddConstantBufferView(UINT ShaderRegister, UINT RegisterSpace, D3D12_ROOT_DESCRIPTOR_FLAGS Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE)
		{
			CD3DX12_ROOT_PARAMETER1 Parameter = {};
			Parameter.InitAsConstantBufferView(ShaderRegister, RegisterSpace, Flags);
			return AddParameter(Parameter);
		}

		RootSignatureDesc& AddShaderResourceView(UINT ShaderRegister, UINT RegisterSpace, D3D12_ROOT_DESCRIPTOR_FLAGS Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE)
		{
			CD3DX12_ROOT_PARAMETER1 Parameter = {};
			Parameter.InitAsShaderResourceView(ShaderRegister, RegisterSpace, Flags);
			return AddParameter(Parameter);
		}

		RootSignatureDesc& AddUnorderedAccessView(UINT ShaderRegister, UINT RegisterSpace, D3D12_ROOT_DESCRIPTOR_FLAGS Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE)
		{
			CD3DX12_ROOT_PARAMETER1 Parameter = {};
			Parameter.InitAsUnorderedAccessView(ShaderRegister, RegisterSpace, Flags);
			return AddParameter(Parameter);
		}

		RootSignatureDesc& AddUnorderedAccessViewWithCounter(UINT ShaderRegister, UINT RegisterSpace, D3D12_DESCRIPTOR_RANGE_FLAGS Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE)
		{
			D3D12DescriptorTable DescriptorTable(1);
			DescriptorTable.AddUAVRange(ShaderRegister, RegisterSpace, 1, Flags);
			return AddDescriptorTable(DescriptorTable);
		}

		RootSignatureDesc& AddRaytracingAccelerationStructure(UINT ShaderRegister, UINT RegisterSpace, D3D12_DESCRIPTOR_RANGE_FLAGS Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE)
		{
			D3D12DescriptorTable DescriptorTable(1);
			DescriptorTable.AddSRVRange(ShaderRegister, RegisterSpace, 1, Flags);
			return AddDescriptorTable(DescriptorTable);
		}

		RootSignatureDesc& AddSampler(
			UINT					   ShaderRegister,
			UINT					   RegisterSpace,
			D3D12_FILTER			   Filter,
			D3D12_TEXTURE_ADDRESS_MODE AddressUVW,
			UINT					   MaxAnisotropy,
			D3D12_COMPARISON_FUNC	   ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL,
			D3D12_STATIC_BORDER_COLOR  BorderColor	  = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE)
		{
			CD3DX12_STATIC_SAMPLER_DESC& Desc = StaticSamplers.emplace_back();
			Desc.Init(ShaderRegister, Filter, AddressUVW, AddressUVW, AddressUVW, 0.0f, MaxAnisotropy, ComparisonFunc, BorderColor);
			Desc.RegisterSpace = RegisterSpace;
			return *this;
		}

		RootSignatureDesc& AllowInputLayout() noexcept;
		RootSignatureDesc& DenyVSAccess() noexcept;
		RootSignatureDesc& DenyPSAccess() noexcept;
		RootSignatureDesc& AsLocalRootSignature() noexcept;
		RootSignatureDesc& DenyASAccess() noexcept;
		RootSignatureDesc& DenyMSAccess() noexcept;

		[[nodiscard]] bool IsLocal() const noexcept;

		D3D12_ROOT_SIGNATURE_DESC1 Build() noexcept;

	private:
		RootSignatureDesc& AddParameter(D3D12_ROOT_PARAMETER1 Parameter);

	private:
		std::vector<CD3DX12_ROOT_PARAMETER1>	 Parameters;
		std::vector<CD3DX12_STATIC_SAMPLER_DESC> StaticSamplers;
		D3D12_ROOT_SIGNATURE_FLAGS				 Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

		std::vector<UINT>				  DescriptorTableIndices;
		std::vector<D3D12DescriptorTable> DescriptorTables;
	};

	class D3D12RootSignature final : public D3D12DeviceChild
	{
	public:
		D3D12RootSignature() noexcept = default;
		explicit D3D12RootSignature(
			D3D12Device*	   Parent,
			RootSignatureDesc& Desc);

		D3D12RootSignature(D3D12RootSignature&&) noexcept = default;
		D3D12RootSignature& operator=(D3D12RootSignature&&) noexcept = default;

		D3D12RootSignature(const D3D12RootSignature&) = delete;
		D3D12RootSignature& operator=(const D3D12RootSignature&) = delete;

		[[nodiscard]] ID3D12RootSignature* GetApiHandle() const noexcept { return RootSignature.Get(); }
		[[nodiscard]] UINT				   GetNumParameters() const noexcept { return NumParameters; }

		[[nodiscard]] std::bitset<KAGUYA_RHI_D3D12_GLOBAL_ROOT_DESCRIPTOR_TABLE_LIMIT> GetDescriptorTableBitMask(D3D12_DESCRIPTOR_HEAP_TYPE Type) const noexcept;

		[[nodiscard]] UINT GetNumDescriptors(UINT RootParameterIndex) const noexcept;

	private:
		// Pre SM6.6 bindless root parameter setup
		static void AddBindlessParameters(RootSignatureDesc& Desc);

	private:
		Arc<ID3D12RootSignature> RootSignature;
		UINT					 NumParameters = 0;

		// Need to know the number of descriptors per descriptor table
		UINT NumDescriptorsPerTable[KAGUYA_RHI_D3D12_GLOBAL_ROOT_DESCRIPTOR_TABLE_LIMIT] = {};

		// A bit mask that represents the root parameter indices that are
		// descriptor tables for CBV, UAV, and SRV
		std::bitset<KAGUYA_RHI_D3D12_GLOBAL_ROOT_DESCRIPTOR_TABLE_LIMIT> ResourceDescriptorTableBitMask;
		// A bit mask that represents the root parameter indices that are
		// descriptor tables for Samplers
		std::bitset<KAGUYA_RHI_D3D12_GLOBAL_ROOT_DESCRIPTOR_TABLE_LIMIT> SamplerTableBitMask;
	};
} // namespace RHI
