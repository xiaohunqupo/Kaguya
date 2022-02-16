#include "D3D12CommandList.h"
#include "D3D12LinkedDevice.h"
#include "D3D12Resource.h"

namespace RHI
{
	D3D12CommandAllocatorPool::D3D12CommandAllocatorPool(
		D3D12LinkedDevice*		Parent,
		D3D12_COMMAND_LIST_TYPE CommandListType) noexcept
		: D3D12LinkedDeviceChild(Parent)
		, CommandListType(CommandListType)
	{
	}

	Arc<ID3D12CommandAllocator> D3D12CommandAllocatorPool::RequestCommandAllocator()
	{
		auto CreateCommandAllocator = [this]()
		{
			Arc<ID3D12CommandAllocator> CommandAllocator;
			VERIFY_D3D12_API(GetParentLinkedDevice()->GetDevice()->CreateCommandAllocator(CommandListType, IID_PPV_ARGS(CommandAllocator.ReleaseAndGetAddressOf())));
			return CommandAllocator;
		};
		auto CommandAllocator = CommandAllocatorPool.RetrieveFromPool(CreateCommandAllocator);
		CommandAllocator->Reset();
		return CommandAllocator;
	}

	void D3D12CommandAllocatorPool::DiscardCommandAllocator(
		Arc<ID3D12CommandAllocator> CommandAllocator,
		D3D12SyncHandle				SyncHandle)
	{
		CommandAllocatorPool.ReturnToPool(std::move(CommandAllocator), SyncHandle);
	}

	void ResourceBarrierBatch::Reset()
	{
		NumResourceBarriers = 0;
	}

	UINT ResourceBarrierBatch::Flush()
	{
		if (NumResourceBarriers > 0)
		{
			GraphicsCommandList->ResourceBarrier(NumResourceBarriers, ResourceBarriers);
			Reset();
		}
		return NumResourceBarriers;
	}

	void ResourceBarrierBatch::Add(
		const D3D12_RESOURCE_BARRIER& ResourceBarrier)
	{
		assert(NumResourceBarriers < NumBatches);
		ResourceBarriers[NumResourceBarriers++] = ResourceBarrier;
		if (NumResourceBarriers == NumBatches)
		{
			Flush();
		}
	}

	void ResourceBarrierBatch::AddTransition(
		D3D12Resource*		  Resource,
		D3D12_RESOURCE_STATES StateBefore,
		D3D12_RESOURCE_STATES StateAfter,
		UINT				  Subresource /*= D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES*/)
	{
		Add(CD3DX12_RESOURCE_BARRIER::Transition(Resource->GetResource(), StateBefore, StateAfter, Subresource));
	}

	void ResourceBarrierBatch::AddAliasing(
		D3D12Resource* BeforeResource,
		D3D12Resource* AfterResource)
	{
		Add(CD3DX12_RESOURCE_BARRIER::Aliasing(BeforeResource->GetResource(), AfterResource->GetResource()));
	}

	void ResourceBarrierBatch::AddUAV(
		D3D12Resource* Resource)
	{
		Add(CD3DX12_RESOURCE_BARRIER::UAV(Resource ? Resource->GetResource() : nullptr));
	}

	std::vector<PendingResourceBarrier>& D3D12ResourceStateTracker::GetPendingResourceBarriers()
	{
		return PendingResourceBarriers;
	}

	CResourceState& D3D12ResourceStateTracker::GetResourceState(D3D12Resource* Resource)
	{
		CResourceState& ResourceState = ResourceStates[Resource];
		// If ResourceState was just created, its state is uninitialized
		if (ResourceState.IsUninitialized())
		{
			ResourceState = CResourceState(Resource->GetNumSubresources());
			ResourceState.SetSubresourceState(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_UNKNOWN);
		}
		return ResourceState;
	}

	void D3D12ResourceStateTracker::Reset()
	{
		ResourceStates.clear();
		PendingResourceBarriers.clear();
	}

	void D3D12ResourceStateTracker::Add(const PendingResourceBarrier& PendingResourceBarrier)
	{
		PendingResourceBarriers.push_back(PendingResourceBarrier);
	}

	D3D12CommandListHandle::D3D12CommandListHandle(
		D3D12LinkedDevice*		Parent,
		D3D12_COMMAND_LIST_TYPE Type)
		: D3D12LinkedDeviceChild(Parent)
		, Type(Type)
	{
		VERIFY_D3D12_API(Parent->GetDevice5()->CreateCommandList1(Parent->GetNodeMask(), Type, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&GraphicsCommandList)));
		GraphicsCommandList->QueryInterface(IID_PPV_ARGS(&GraphicsCommandList4));
		GraphicsCommandList->QueryInterface(IID_PPV_ARGS(&GraphicsCommandList6));
#ifdef LUNA_D3D12_DEBUG_RESOURCE_STATES
		GraphicsCommandList->QueryInterface(IID_PPV_ARGS(&DebugCommandList));
#endif
		ResourceBarrierBatch.GraphicsCommandList = GraphicsCommandList.Get();
	}

	void D3D12CommandListHandle::Open(ID3D12CommandAllocator* CommandAllocator)
	{
		VERIFY_D3D12_API(GraphicsCommandList->Reset(CommandAllocator, nullptr));

		// Reset resource state tracking and resource barriers
		ResourceStateTracker.Reset();
		ResourceBarrierBatch.Reset();
	}

	void D3D12CommandListHandle::Close()
	{
		FlushResourceBarriers();
		VERIFY_D3D12_API(GraphicsCommandList->Close());
	}

	void D3D12CommandListHandle::TransitionBarrier(
		D3D12Resource*		  Resource,
		D3D12_RESOURCE_STATES State,
		UINT				  Subresource /*= D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES*/)
	{
		// TODO: There might be some logic and cases here im missing, come back and edit if anything goes boom

		CResourceState& ResourceState = ResourceStateTracker.GetResourceState(Resource);
		// First use on the command list
		if (ResourceState.IsUnknown())
		{
			PendingResourceBarrier PendingResourceBarrier = {
				.Resource	 = Resource,
				.State		 = State,
				.Subresource = Subresource
			};
			ResourceStateTracker.Add(PendingResourceBarrier);
		}
		// Known state within the command list
		else
		{
			// If we are applying transition to all subresources and we are in different tracking mode
			// transition each subresource individually
			if (Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES && !ResourceState.IsUniform())
			{
				// First transition all of the subresources if they are different than the State
				UINT i = 0;
				for (D3D12_RESOURCE_STATES SubresourceState : ResourceState)
				{
					if (SubresourceState != State)
					{
						ResourceBarrierBatch.AddTransition(Resource, SubresourceState, State, i);
					}

					i++;
				}
			}
			// Apply barrier at subresource level
			else
			{
				D3D12_RESOURCE_STATES StateKnown = ResourceState.GetSubresourceState(Subresource);
				if (StateKnown != State)
				{
					ResourceBarrierBatch.AddTransition(Resource, StateKnown, State, Subresource);
				}
			}
		}

		// Update resource state, potentially change state tracking mode
		ResourceState.SetSubresourceState(Subresource, State);
	}

	void D3D12CommandListHandle::AliasingBarrier(
		D3D12Resource* BeforeResource,
		D3D12Resource* AfterResource)
	{
		ResourceBarrierBatch.AddAliasing(BeforeResource, AfterResource);
	}

	void D3D12CommandListHandle::UAVBarrier(
		D3D12Resource* Resource)
	{
		ResourceBarrierBatch.AddUAV(Resource);
	}

	void D3D12CommandListHandle::FlushResourceBarriers()
	{
		ResourceBarrierBatch.Flush();
	}

	bool D3D12CommandListHandle::AssertResourceState(
		D3D12Resource*		  Resource,
		D3D12_RESOURCE_STATES State,
		UINT				  Subresource)
	{
#ifdef LUNA_D3D12_DEBUG_RESOURCE_STATES
		if (DebugCommandList)
		{
			return DebugCommandList->AssertResourceState(Resource->GetResource(), Subresource, State);
		}
#endif
		return true;
	}

	std::vector<D3D12_RESOURCE_BARRIER> D3D12CommandListHandle::ResolveResourceBarriers()
	{
		const auto& PendingResourceBarriers = ResourceStateTracker.GetPendingResourceBarriers();

		std::vector<D3D12_RESOURCE_BARRIER> ResourceBarriers;
		ResourceBarriers.reserve(PendingResourceBarriers.size());

		for (const auto& [Resource, State, Subresource] : PendingResourceBarriers)
		{
			CResourceState& ResourceState = Resource->GetResourceState();

			D3D12_RESOURCE_STATES StateBefore = ResourceState.GetSubresourceState(Subresource);
			D3D12_RESOURCE_STATES StateAfter  = State != D3D12_RESOURCE_STATE_UNKNOWN ? State : StateBefore;

			if (StateBefore != StateAfter)
			{
				ResourceBarriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(Resource->GetResource(), StateBefore, StateAfter, Subresource));
			}

			// Get the command list resource state associate with this resource
			D3D12_RESOURCE_STATES StateCommandList = ResourceStateTracker.GetResourceState(Resource).GetSubresourceState(Subresource);
			D3D12_RESOURCE_STATES StatePrevious	   = StateCommandList != D3D12_RESOURCE_STATE_UNKNOWN ? StateCommandList : StateAfter;

			if (StateBefore != StatePrevious)
			{
				ResourceState.SetSubresourceState(Subresource, StatePrevious);
			}
		}

		return ResourceBarriers;
	}
} // namespace RHI
