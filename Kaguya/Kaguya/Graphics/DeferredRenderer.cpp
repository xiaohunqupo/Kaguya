#include "DeferredRenderer.h"
#include "RendererRegistry.h"

void* DeferredRenderer::GetViewportDescriptor()
{
	if (ValidViewport)
	{
		return reinterpret_cast<void*>(Registry.GetTextureSRV(Viewport).GetGpuHandle().ptr);
	}

	return nullptr;
}

void DeferredRenderer::SetViewportResolution(uint32_t Width, uint32_t Height)
{
	if (Resolution.ViewportWidth != Width || Resolution.ViewportHeight != Height)
	{
		Resolution.ViewportWidth			 = Width;
		Resolution.ViewportHeight			 = Height;
		Resolution.ViewportResolutionResized = true;
	}

	ValidViewport = false;

	if (Resolution.RenderWidth != Width || Resolution.RenderHeight != Height)
	{
		Resolution.RenderWidth			   = Width;
		Resolution.RenderHeight			   = Height;
		Resolution.RenderResolutionResized = true;
	}
}

void DeferredRenderer::Initialize()
{
	Shaders::Compile();
	// Libraries::Compile();
	RootSignatures::Compile(RenderDevice);
	PipelineStates::Compile(RenderDevice);
	// RaytracingPipelineStates::Compile(RenderDevice);

	{
		RenderPassDesc Desc		= {};
		FLOAT		   Color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		Desc.AddRenderTarget({ .Format	   = ERHIFormat::RGBA32_FLOAT,
							   .LoadOp	   = ELoadOp::Clear,
							   .StoreOp	   = EStoreOp::Store,
							   .ClearValue = ClearValue(ERHIFormat::RGBA32_FLOAT, Color) });
		Desc.AddRenderTarget({ .Format	   = ERHIFormat::RGBA32_FLOAT,
							   .LoadOp	   = ELoadOp::Clear,
							   .StoreOp	   = EStoreOp::Store,
							   .ClearValue = ClearValue(ERHIFormat::RGBA32_FLOAT, Color) });
		Desc.AddRenderTarget({ .Format	   = ERHIFormat::RG16_FLOAT,
							   .LoadOp	   = ELoadOp::Clear,
							   .StoreOp	   = EStoreOp::Store,
							   .ClearValue = ClearValue(ERHIFormat::RG16_FLOAT, Color) });
		Desc.SetDepthStencil({ .Format	   = ERHIFormat::D32,
							   .LoadOp	   = ELoadOp::Clear,
							   .StoreOp	   = EStoreOp::Store,
							   .ClearValue = ClearValue(ERHIFormat::D32, 1.0f, 0xFF) });
		GBufferRenderPass = D3D12RenderPass(RenderCore::Device, Desc);
	}

	Materials = D3D12Buffer(
		RenderCore::Device->GetDevice(),
		sizeof(HLSL::Material) * World::MaterialLimit,
		sizeof(HLSL::Material),
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_FLAG_NONE);
	Materials.Initialize();
	pMaterial = Materials.GetCpuVirtualAddress<HLSL::Material>();

	Lights = D3D12Buffer(
		RenderCore::Device->GetDevice(),
		sizeof(HLSL::Light) * World::LightLimit,
		sizeof(HLSL::Light),
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_FLAG_NONE);
	Lights.Initialize();
	pLights = Lights.GetCpuVirtualAddress<HLSL::Light>();

	Meshes = D3D12Buffer(
		RenderCore::Device->GetDevice(),
		sizeof(HLSL::Mesh) * World::InstanceLimit,
		sizeof(HLSL::Mesh),
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_FLAG_NONE);
	Meshes.Initialize();
	pMeshes = Meshes.GetCpuVirtualAddress<HLSL::Mesh>();

	MeshRenderers.reserve(World::InstanceLimit);
	HlslMeshes.resize(World::InstanceLimit);
}

void DeferredRenderer::Render(World* World, D3D12CommandContext& Context)
{
	if (ImGui::Begin("GBuffer"))
	{
		constexpr const char* View[] = { "Albedo", "Normal", "Motion", "Depth"};
		ImGui::Combo("View", &ViewMode, View, static_cast<int>(std::size(View)));

		ImGui::Text("Render resolution: %dx%d", Resolution.RenderWidth, Resolution.RenderHeight);
		ImGui::Text("Viewport resolution: %dx%d", Resolution.ViewportWidth, Resolution.ViewportHeight);
	}
	ImGui::End();

	MeshRenderers.clear();

	NumMaterials = NumLights = NumMeshes = 0;
	World->Registry.view<Transform, MeshFilter, MeshRenderer>().each(
		[&](auto&& Transform, auto&& MeshFilter, auto&& MeshRenderer)
		{
			if (MeshFilter.Mesh)
			{
				pMaterial[NumMaterials]					= GetHLSLMaterialDesc(MeshRenderer.Material);
				HLSL::Mesh Mesh							= GetHLSLMeshDesc(Transform);
				HlslMeshes[NumMeshes].PreviousTransform = HlslMeshes[NumMeshes].Transform;
				HlslMeshes[NumMeshes].Transform			= Mesh.Transform;
				HlslMeshes[NumMeshes].MaterialIndex		= NumMaterials;
				MeshRenderers.push_back(&MeshRenderer);

				++NumMaterials;
				++NumMeshes;
			}
		});
	World->Registry.view<Transform, Light>().each(
		[&](auto&& Transform, auto&& Light)
		{
			pLights[NumLights++] = GetHLSLLightDesc(Transform, Light);
		});
	std::memcpy(pMeshes, HlslMeshes.data(), sizeof(HLSL::Mesh) * World::InstanceLimit);

	if (World->WorldState & EWorldState::EWorldState_Update)
	{
		World->WorldState = EWorldState_Render;
	}

	RenderGraph RenderGraph(Allocator, Scheduler, Registry, Resolution);

	struct GBufferParams
	{
		RenderResourceHandle Albedo;
		RenderResourceHandle Normal;
		RenderResourceHandle Motion;
		RenderResourceHandle Depth;
		RenderResourceHandle RenderTarget;
	};
	RenderPass* GBuffer = RenderGraph.AddRenderPass(
		"GBuffer",
		[&](RenderGraphScheduler& Scheduler, RenderScope& Scope)
		{
			FLOAT Color[] = { 0, 0, 0, 0 };

			auto&		Params	 = Scope.Get<GBufferParams>();
			const auto& ViewData = Scope.Get<RenderGraphViewData>();

			Params.Albedo = Scheduler.CreateTexture(
				"Albedo",
				RGTextureDesc::Texture2D(
					ETextureResolution::Render,
					DXGI_FORMAT_R32G32B32A32_FLOAT,
					0,
					0,
					1,
					TextureFlag_AllowRenderTarget,
					CD3DX12_CLEAR_VALUE(DXGI_FORMAT_R32G32B32A32_FLOAT, Color)));
			Params.Normal = Scheduler.CreateTexture(
				"Normal",
				RGTextureDesc::Texture2D(
					ETextureResolution::Render,
					DXGI_FORMAT_R32G32B32A32_FLOAT,
					0,
					0,
					1,
					TextureFlag_AllowRenderTarget,
					CD3DX12_CLEAR_VALUE(DXGI_FORMAT_R32G32B32A32_FLOAT, Color)));
			Params.Motion = Scheduler.CreateTexture(
				"Motion",
				RGTextureDesc::Texture2D(
					ETextureResolution::Render,
					DXGI_FORMAT_R16G16_FLOAT,
					0,
					0,
					1,
					TextureFlag_AllowRenderTarget,
					CD3DX12_CLEAR_VALUE(DXGI_FORMAT_R16G16_FLOAT, Color)));
			Params.Depth = Scheduler.CreateTexture(
				"Depth",
				RGTextureDesc::Texture2D(
					ETextureResolution::Render,
					DXGI_FORMAT_D32_FLOAT,
					0,
					0,
					1,
					TextureFlag_AllowDepthStencil,
					CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 1.0f, 0xFF)));

			{
				RGRenderTargetDesc Desc = {};
				Desc.AddRenderTarget(Params.Albedo, false);
				Desc.AddRenderTarget(Params.Normal, false);
				Desc.AddRenderTarget(Params.Motion, false);
				Desc.SetDepthStencil(Params.Depth);

				Params.RenderTarget = Scheduler.CreateRenderTarget(Desc);
			}

			return [=, &Params, &ViewData, this](RenderGraphRegistry& Registry, D3D12CommandContext& Context)
			{
				D3D12ScopedEvent(Context, "GBuffer");

				_declspec(align(256)) struct GlobalConstants
				{
					HLSL::Camera CurrentCamera;
					HLSL::Camera PreviousCamera;

					unsigned int NumLights;
				} g_GlobalConstants				 = {};
				g_GlobalConstants.CurrentCamera	 = GetHLSLCameraDesc(*World->ActiveCamera);
				g_GlobalConstants.PreviousCamera = GetHLSLCameraDesc(*World->ActiveCamera);
				g_GlobalConstants.NumLights		 = NumLights;

				Context.SetPipelineState(RenderDevice.GetPipelineState(PipelineStates::GBuffer));
				Context.SetGraphicsRootSignature(RenderDevice.GetRootSignature(RootSignatures::GBuffer));
				Context.SetGraphicsConstantBuffer(1, sizeof(GlobalConstants), &g_GlobalConstants);
				Context->SetGraphicsRootShaderResourceView(2, Materials.GetGpuVirtualAddress());
				Context->SetGraphicsRootShaderResourceView(3, Lights.GetGpuVirtualAddress());
				Context->SetGraphicsRootShaderResourceView(4, Meshes.GetGpuVirtualAddress());

				Context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				Context.SetViewport(
					RHIViewport(0.0f, 0.0f, ViewData.GetRenderWidth<FLOAT>(), ViewData.GetRenderHeight<FLOAT>()));
				Context.SetScissorRect(RHIRect(0, 0, ViewData.RenderWidth, ViewData.RenderHeight));

				Context.BeginRenderPass(&GBufferRenderPass, &Registry.GetRenderTarget(Params.RenderTarget));
				{
					for (auto [i, MeshRenderer] : enumerate(MeshRenderers))
					{
						Context->SetGraphicsRoot32BitConstant(0, i, 0);

						D3D12Buffer& VertexBuffer = MeshRenderer->pMeshFilter->Mesh->VertexResource;
						D3D12Buffer& IndexBuffer  = MeshRenderer->pMeshFilter->Mesh->IndexResource;

						D3D12_VERTEX_BUFFER_VIEW VertexBufferView = {};
						VertexBufferView.BufferLocation			  = VertexBuffer.GetGpuVirtualAddress();
						VertexBufferView.SizeInBytes			  = VertexBuffer.GetDesc().Width;
						VertexBufferView.StrideInBytes			  = VertexBuffer.GetStride();
						Context->IASetVertexBuffers(0, 1, &VertexBufferView);

						D3D12_INDEX_BUFFER_VIEW IndexBufferView = {};
						IndexBufferView.BufferLocation			= IndexBuffer.GetGpuVirtualAddress();
						IndexBufferView.SizeInBytes				= IndexBuffer.GetDesc().Width;
						IndexBufferView.Format					= DXGI_FORMAT_R32_UINT;
						Context->IASetIndexBuffer(&IndexBufferView);

						Context->DrawIndexedInstanced(MeshRenderer->pMeshFilter->Mesh->Indices.size(), 1, 0, 0, 0);
					}
				}
				Context.EndRenderPass();
			};
		});

	RenderResourceHandle Views[] = {
		GBuffer->Scope.Get<GBufferParams>().Albedo,
		GBuffer->Scope.Get<GBufferParams>().Normal,
		GBuffer->Scope.Get<GBufferParams>().Motion,
		GBuffer->Scope.Get<GBufferParams>().Depth,
	};

	Viewport	  = Views[ViewMode];
	ValidViewport = true;

	RenderGraph.Setup();
	RenderGraph.Compile();
	RenderGraph.Execute(Context);

	if (ImGui::Begin("Render Graph"))
	{
		for (const auto& RenderPass : RenderGraph)
		{
			char Label[MAX_PATH] = {};
			sprintf_s(Label, "Pass: %s", RenderPass->Name.data());
			if (ImGui::TreeNode(Label))
			{
				constexpr ImGuiTableFlags TableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
													   ImGuiTableFlags_Hideable | ImGuiTableFlags_RowBg |
													   ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV;

				ImGui::Text("Inputs");
				if (ImGui::BeginTable("Inputs", 1, TableFlags))
				{
					for (auto Handle : RenderPass->Reads)
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);

						ImGui::Text("%s", RenderGraph.GetScheduler().GetTextureName(Handle).data());
					}
					ImGui::EndTable();
				}

				ImGui::Text("Outputs");
				if (ImGui::BeginTable("Outputs", 1, TableFlags))
				{
					for (auto Handle : RenderPass->Writes)
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);

						ImGui::Text("%s", RenderGraph.GetScheduler().GetTextureName(Handle).data());
					}
					ImGui::EndTable();
				}

				ImGui::TreePop();
			}
		}
	}
	ImGui::End();
}
