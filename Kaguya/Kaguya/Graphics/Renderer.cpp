#include "Renderer.h"
#include "World/Entity.h"
#include "RendererRegistry.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

void Renderer::OnSetViewportResolution(uint32_t Width, uint32_t Height)
{
	if (Width == 0 || Height == 0)
	{
		return;
	}

	ViewportWidth  = Width;
	ViewportHeight = Height;

	SetViewportResolution(Width, Height);

	RenderGraph.SetResolution(RenderWidth, RenderHeight, ViewportWidth, ViewportHeight);
	RenderGraph.GetRegistry().RealizeResources();
	RenderGraph.ValidViewport = false;
}

void Renderer::OnInitialize()
{
	Initialize();
}

void Renderer::OnRender()
{
	pWorld->ActiveCamera->AspectRatio = float(RenderWidth) / float(RenderHeight);

	RenderCore::pAdapter->OnBeginFrame();

	if (ImGui::Begin("GPU Timing"))
	{
		for (const auto& iter : Profiler::Data)
		{
			for (INT i = 0; i < iter.Depth; ++i)
			{
				ImGui::Text("    ");
				ImGui::SameLine();
			}
			ImGui::Text("%s: %.2fms (%.2fms max)", iter.Name, iter.AvgTime, iter.MaxTime);
			ImGui::SameLine();
			ImGui::NewLine();
		}
	}
	ImGui::End();

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

	CommandContext& Context = RenderCore::pAdapter->GetDevice()->GetCommandContext();
	Context.OpenCommandList();
	Context.BindResourceViewHeaps();

	Render(Context);

	auto [pRenderTarget, RenderTargetView] = RenderCore::pSwapChain->GetCurrentBackBufferResource();

	auto Barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		pRenderTarget,
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET);
	Context->ResourceBarrier(1, &Barrier);
	{
		auto Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, float(Application::GetWidth()), float(Application::GetHeight()));
		auto ScissorRect = CD3DX12_RECT(0, 0, Application::GetWidth(), Application::GetHeight());

		Context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		Context->RSSetViewports(1, &Viewport);
		Context->RSSetScissorRects(1, &ScissorRect);
		Context->OMSetRenderTargets(1, &RenderTargetView, TRUE, nullptr);
		FLOAT white[] = { 1, 1, 1, 1 };
		Context->ClearRenderTargetView(RenderTargetView, white, 0, nullptr);

		// ImGui Render
		{
			D3D12ScopedEvent(Context, "ImGui Render");

			ImGui::Render();
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), Context.CommandListHandle.GetGraphicsCommandList());
		}
	}
	Barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		pRenderTarget,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT);
	Context->ResourceBarrier(1, &Barrier);

	Context.CloseCommandList();
	CommandSyncPoint MainSyncPoint = Context.Execute(false);

	RenderCore::pSwapChain->Present(false);

	RenderCore::pAdapter->OnEndFrame();

	MainSyncPoint.WaitForCompletion();
}

void Renderer::OnResize(uint32_t Width, uint32_t Height)
{
	RenderCore::pAdapter->GetDevice()->GetGraphicsQueue()->Flush();
	RenderCore::pSwapChain->Resize(Width, Height);
}

void Renderer::OnDestroy()
{
	PipelineStates::Destroy();
	RaytracingPipelineStates::Destroy();
}
