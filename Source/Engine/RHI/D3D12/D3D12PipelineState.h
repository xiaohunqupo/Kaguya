#pragma once
#include "D3D12Types.h"

namespace RHI
{
	template<typename TDesc>
	struct D3D12PipelineStateTraits
	{
	};
	template<>
	struct D3D12PipelineStateTraits<D3D12_GRAPHICS_PIPELINE_STATE_DESC>
	{
		static auto Create() { return &ID3D12Device::CreateGraphicsPipelineState; }
	};
	template<>
	struct D3D12PipelineStateTraits<D3D12_COMPUTE_PIPELINE_STATE_DESC>
	{
		static auto Create() { return &ID3D12Device::CreateComputePipelineState; }
	};
	template<>
	struct D3D12PipelineStateTraits<D3D12_PIPELINE_STATE_STREAM_DESC>
	{
		static auto Create() { return &ID3D12Device2::CreatePipelineState; }
	};

	class D3D12PipelineParserCallbacks : public IPipelineParserCallbacks
	{
	public:
		void RootSignatureCb(D3D12RootSignature* RootSignature) override;
		void InputLayoutCb(D3D12InputLayout* InputLayout) override;
		void VSCb(Shader* VS) override;
		void PSCb(Shader* PS) override;
		void CSCb(Shader* CS) override;
		void ASCb(Shader* AS) override;
		void MSCb(Shader* MS) override;
		void BlendStateCb(const RHIBlendState& BlendState) override;
		void RasterizerStateCb(const RHIRasterizerState& RasterizerState) override;
		void DepthStencilStateCb(const RHIDepthStencilState& DepthStencilState) override;
		void RenderTargetStateCb(const RHIRenderTargetState& RenderTargetState) override;
		void PrimitiveTopologyTypeCb(RHI_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopology) override;

		void ErrorBadInputParameter(size_t Index) override;
		void ErrorDuplicateSubobject(RHI_PIPELINE_STATE_SUBOBJECT_TYPE Type) override;
		void ErrorUnknownSubobject(size_t Index) override;

		RHI_PIPELINE_STATE_TYPE				  Type;
		bool								  ContainsMeshShader = false;
		ID3D12RootSignature*				  RootSignature		 = nullptr;
		std::vector<D3D12_INPUT_ELEMENT_DESC> InputElements;
		D3D12_SHADER_BYTECODE				  VS				= {};
		D3D12_SHADER_BYTECODE				  PS				= {};
		D3D12_SHADER_BYTECODE				  CS				= {};
		D3D12_SHADER_BYTECODE				  AS				= {};
		D3D12_SHADER_BYTECODE				  MS				= {};
		D3D12_BLEND_DESC					  BlendState		= CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		D3D12_RASTERIZER_DESC				  RasterizerState	= CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		D3D12_DEPTH_STENCIL_DESC			  DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		DXGI_FORMAT							  RTFormats[8]		= {};
		UINT								  NumRenderTargets	= 0;
		DXGI_FORMAT							  DSFormat			= DXGI_FORMAT_UNKNOWN;
		D3D12_PRIMITIVE_TOPOLOGY_TYPE		  PrimitiveTopology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
	};

	class D3D12PipelineState : public D3D12DeviceChild
	{
	public:
		D3D12PipelineState() noexcept = default;
		explicit D3D12PipelineState(
			D3D12Device*				   Parent,
			std::wstring				   Name,
			const PipelineStateStreamDesc& Desc);

		D3D12PipelineState(D3D12PipelineState&&) noexcept			 = default;
		D3D12PipelineState& operator=(D3D12PipelineState&&) noexcept = default;

		D3D12PipelineState(const D3D12RootSignature&)			 = delete;
		D3D12PipelineState& operator=(const D3D12PipelineState&) = delete;

		[[nodiscard]] ID3D12PipelineState* GetApiHandle() const noexcept;

	private:
		static AsyncTask<Arc<ID3D12PipelineState>> Create(
			D3D12Device*				 Device,
			std::wstring				 Name,
			RHI_PIPELINE_STATE_TYPE		 Type,
			D3D12PipelineParserCallbacks Parser);

		template<typename TDesc>
		static Arc<ID3D12PipelineState> Compile(
			D3D12Device*		Device,
			const std::wstring& Name,
			const TDesc&		Desc);

	private:
		mutable Arc<ID3D12PipelineState>			PipelineState;
		mutable AsyncTask<Arc<ID3D12PipelineState>> CompilationWork;
	};

	struct DxilLibrary
	{
		DxilLibrary(
			const D3D12_SHADER_BYTECODE&		  Library,
			const std::vector<std::wstring_view>& Symbols)
			: Library(Library)
			, Symbols(Symbols)
		{
		}

		D3D12_SHADER_BYTECODE		   Library;
		std::vector<std::wstring_view> Symbols;
	};

	struct HitGroup
	{
		HitGroup(
			std::wstring_view				 HitGroupName,
			std::optional<std::wstring_view> AnyHitSymbol,
			std::optional<std::wstring_view> ClosestHitSymbol,
			std::optional<std::wstring_view> IntersectionSymbol)
			: HitGroupName(HitGroupName)
			, AnyHitSymbol(AnyHitSymbol ? *AnyHitSymbol : std::wstring_view{})
			, ClosestHitSymbol(ClosestHitSymbol ? *ClosestHitSymbol : std::wstring_view{})
			, IntersectionSymbol(IntersectionSymbol ? *IntersectionSymbol : std::wstring_view{})
		{
		}

		std::wstring_view HitGroupName;
		std::wstring_view AnyHitSymbol;
		std::wstring_view ClosestHitSymbol;
		std::wstring_view IntersectionSymbol;
	};

	struct RootSignatureAssociation
	{
		RootSignatureAssociation(
			ID3D12RootSignature*				  RootSignature,
			const std::vector<std::wstring_view>& Symbols)
			: RootSignature(RootSignature)
			, Symbols(Symbols)
		{
		}

		ID3D12RootSignature*		   RootSignature;
		std::vector<std::wstring_view> Symbols;
	};

	class RaytracingPipelineStateDesc
	{
	public:
		RaytracingPipelineStateDesc() noexcept;

		RaytracingPipelineStateDesc& AddLibrary(
			const D3D12_SHADER_BYTECODE&		  Library,
			const std::vector<std::wstring_view>& Symbols);

		RaytracingPipelineStateDesc& AddHitGroup(
			std::wstring_view				 HitGroupName,
			std::optional<std::wstring_view> AnyHitSymbol,
			std::optional<std::wstring_view> ClosestHitSymbol,
			std::optional<std::wstring_view> IntersectionSymbol);

		RaytracingPipelineStateDesc& AddRootSignatureAssociation(
			ID3D12RootSignature*				  RootSignature,
			const std::vector<std::wstring_view>& Symbols);

		RaytracingPipelineStateDesc& SetGlobalRootSignature(
			ID3D12RootSignature* GlobalRootSignature);

		RaytracingPipelineStateDesc& SetRaytracingShaderConfig(
			UINT MaxPayloadSizeInBytes,
			UINT MaxAttributeSizeInBytes);

		RaytracingPipelineStateDesc& SetRaytracingPipelineConfig(
			UINT MaxTraceRecursionDepth);

		D3D12_STATE_OBJECT_DESC Build();

	private:
		[[nodiscard]] std::vector<std::wstring_view> BuildShaderExportList() const;

	private:
		CD3DX12_STATE_OBJECT_DESC Desc;

		std::vector<DxilLibrary>			  Libraries;
		std::vector<HitGroup>				  HitGroups;
		std::vector<RootSignatureAssociation> RootSignatureAssociations;
		ID3D12RootSignature*				  GlobalRootSignature;
		D3D12_RAYTRACING_SHADER_CONFIG		  ShaderConfig;
		D3D12_RAYTRACING_PIPELINE_CONFIG	  PipelineConfig;
	};

	class D3D12RaytracingPipelineState : public D3D12DeviceChild
	{
	public:
		D3D12RaytracingPipelineState() noexcept = default;
		explicit D3D12RaytracingPipelineState(
			D3D12Device*				 Parent,
			RaytracingPipelineStateDesc& Desc);

		D3D12RaytracingPipelineState(D3D12RaytracingPipelineState&&) noexcept			 = default;
		D3D12RaytracingPipelineState& operator=(D3D12RaytracingPipelineState&&) noexcept = default;

		D3D12RaytracingPipelineState(const D3D12RaytracingPipelineState&)			 = delete;
		D3D12RaytracingPipelineState& operator=(const D3D12RaytracingPipelineState&) = delete;

		[[nodiscard]] ID3D12StateObject* GetApiHandle() const { return StateObject.Get(); }
		[[nodiscard]] void*				 GetShaderIdentifier(std::wstring_view ExportName) const;

	private:
		Arc<ID3D12StateObject>			 StateObject;
		Arc<ID3D12StateObjectProperties> StateObjectProperties;
	};
} // namespace RHI
