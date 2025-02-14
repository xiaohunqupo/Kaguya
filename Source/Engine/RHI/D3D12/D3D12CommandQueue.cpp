#include "D3D12CommandQueue.h"
#include "D3D12LinkedDevice.h"

namespace RHI
{
	D3D12CommandQueue::D3D12CommandQueue(
		D3D12LinkedDevice*		 Parent,
		RHID3D12CommandQueueType Type)
		: D3D12LinkedDeviceChild(Parent)
		, CommandListType([&]()
						  {
							  switch (Type)
							  {
								  using enum RHID3D12CommandQueueType;
							  case Direct:
								  return D3D12_COMMAND_LIST_TYPE_DIRECT;
							  case AsyncCompute:
								  return D3D12_COMMAND_LIST_TYPE_COMPUTE;
							  case Copy:
								  [[fallthrough]];
							  case Upload:
								  return D3D12_COMMAND_LIST_TYPE_COPY;
							  }
							  return D3D12_COMMAND_LIST_TYPE();
						  }())
		, CommandQueue([&]
					   {
						   Arc<ID3D12CommandQueue>		  CommandQueue;
						   const D3D12_COMMAND_QUEUE_DESC Desc = {
							   .Type	 = CommandListType,
							   .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
							   .Flags	 = D3D12_COMMAND_QUEUE_FLAG_NONE,
							   .NodeMask = Parent->GetNodeMask()
						   };
						   VERIFY_D3D12_API(Parent->GetDevice()->CreateCommandQueue(&Desc, IID_PPV_ARGS(&CommandQueue)));
						   return CommandQueue;
					   }())
		, Fence(Parent->GetParentDevice(), 0, D3D12_FENCE_FLAG_NONE)
		, ResourceBarrierCommandAllocatorPool(Parent, CommandListType)
		, ResourceBarrierCommandAllocator(ResourceBarrierCommandAllocatorPool.RequestCommandAllocator())
		, ResourceBarrierCommandListHandle(Parent, CommandListType)
	{
#ifdef _DEBUG
		switch (Type)
		{
			using enum RHID3D12CommandQueueType;
		case Direct:
			CommandQueue->SetName(L"3D");
			Fence.Get()->SetName(L"3D Fence");
			break;
		case AsyncCompute:
			CommandQueue->SetName(L"Async Compute");
			Fence.Get()->SetName(L"Async Compute Fence");
			break;
		case Copy:
			CommandQueue->SetName(L"Copy 1");
			Fence.Get()->SetName(L"Copy 1 Fence");
			break;
		case Upload:
			CommandQueue->SetName(L"Copy 2");
			Fence.Get()->SetName(L"Copy 2 Fence");
			break;
		}
#endif
		UINT64 Frequency = 0;
		if (SUCCEEDED(CommandQueue->GetTimestampFrequency(&Frequency)))
		{
			this->Frequency = Frequency;
		}
	}

	UINT64 D3D12CommandQueue::Signal()
	{
		return Fence.Signal(this);
	}

	bool D3D12CommandQueue::IsFenceComplete(UINT64 FenceValue)
	{
		return Fence.IsFenceComplete(FenceValue);
	}

	void D3D12CommandQueue::HostWaitForValue(UINT64 FenceValue)
	{
		Fence.HostWaitForValue(FenceValue);
	}

	void D3D12CommandQueue::Wait(D3D12CommandQueue* CommandQueue) const
	{
		WaitForSyncHandle(CommandQueue->SyncHandle);
	}

	void D3D12CommandQueue::WaitForSyncHandle(const D3D12SyncHandle& SyncHandle) const
	{
		if (SyncHandle)
		{
			VERIFY_D3D12_API(CommandQueue->Wait(SyncHandle.Fence->Get(), SyncHandle.Value));
		}
	}

	D3D12SyncHandle D3D12CommandQueue::ExecuteCommandLists(
		Span<D3D12CommandListHandle* const> CommandListHandles,
		bool								WaitForCompletion)
	{
		UINT			   NumCommandLists		 = 0;
		UINT			   NumBarrierCommandList = 0;
		ID3D12CommandList* CommandLists[32]		 = {};

		// Resolve resource barriers
		for (const auto& CommandListHandle : CommandListHandles)
		{
			if (ResolveResourceBarrierCommandList(CommandListHandle))
			{
				CommandLists[NumCommandLists++] = ResourceBarrierCommandListHandle.GetCommandList();
				NumBarrierCommandList++;
			}

			CommandLists[NumCommandLists++] = CommandListHandle->GetCommandList();
		}

		CommandQueue->ExecuteCommandLists(NumCommandLists, CommandLists);
		UINT64 FenceValue = Signal();
		SyncHandle		  = D3D12SyncHandle(&Fence, FenceValue);

		// Discard command allocator used exclusively to resolve resource barriers
		if (NumBarrierCommandList > 0)
		{
			ResourceBarrierCommandAllocatorPool.DiscardCommandAllocator(std::exchange(ResourceBarrierCommandAllocator, {}), SyncHandle);
		}

		if (WaitForCompletion)
		{
			HostWaitForValue(FenceValue);
			assert(SyncHandle.IsComplete());
		}

		return SyncHandle;
	}

	bool D3D12CommandQueue::ResolveResourceBarrierCommandList(D3D12CommandListHandle* CommandListHandle)
	{
		std::vector<D3D12_RESOURCE_BARRIER> ResourceBarriers = CommandListHandle->ResolveResourceBarriers();

		bool AnyResolved = !ResourceBarriers.empty();
		if (AnyResolved)
		{
			if (!ResourceBarrierCommandAllocator)
			{
				ResourceBarrierCommandAllocator = ResourceBarrierCommandAllocatorPool.RequestCommandAllocator();
			}

			ResourceBarrierCommandListHandle.Open(ResourceBarrierCommandAllocator);
			ResourceBarrierCommandListHandle->ResourceBarrier(static_cast<UINT>(ResourceBarriers.size()), ResourceBarriers.data());
			ResourceBarrierCommandListHandle.Close();
		}

		return AnyResolved;
	}
} // namespace RHI
