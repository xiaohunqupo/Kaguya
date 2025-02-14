#pragma once
#include "D3D12Types.h"
#include "D3D12CommandQueue.h"
#include "D3D12Profiler.h"
#include "D3D12CommandContext.h"
#include "D3D12Descriptor.h"
#include "D3D12DescriptorHeap.h"

namespace RHI
{
	class D3D12LinkedDevice : public D3D12DeviceChild
	{
	public:
		explicit D3D12LinkedDevice(
			D3D12Device*  Parent,
			D3D12NodeMask NodeMask);
		~D3D12LinkedDevice();

		[[nodiscard]] ID3D12Device*		   GetDevice() const noexcept;
		[[nodiscard]] ID3D12Device5*	   GetDevice5() const noexcept;
		[[nodiscard]] D3D12NodeMask		   GetNodeMask() const noexcept { return NodeMask; }
		[[nodiscard]] D3D12CommandQueue*   GetCommandQueue(RHID3D12CommandQueueType Type) noexcept;
		[[nodiscard]] D3D12CommandQueue*   GetGraphicsQueue() noexcept { return &GraphicsQueue; }
		[[nodiscard]] D3D12CommandQueue*   GetAsyncComputeQueue() noexcept { return &AsyncComputeQueue; }
		[[nodiscard]] D3D12CommandQueue*   GetCopyQueue() noexcept { return &CopyQueue; }
		[[nodiscard]] D3D12Profiler*	   GetProfiler() noexcept { return &Profiler; }
		[[nodiscard]] D3D12DescriptorHeap& GetResourceDescriptorHeap() noexcept { return ResourceDescriptorHeap; }
		[[nodiscard]] D3D12DescriptorHeap& GetSamplerDescriptorHeap() noexcept { return SamplerDescriptorHeap; }
		// clang-format off
		template<typename ViewDesc> CDescriptorHeapManager* GetHeapManager() noexcept;
		template<> CDescriptorHeapManager* GetHeapManager<D3D12_RENDER_TARGET_VIEW_DESC>() noexcept { return &RtvHeapManager; }
		template<> CDescriptorHeapManager* GetHeapManager<D3D12_DEPTH_STENCIL_VIEW_DESC>() noexcept { return &DsvHeapManager; }
		template<> CDescriptorHeapManager* GetHeapManager<D3D12_CONSTANT_BUFFER_VIEW_DESC>() noexcept { return nullptr; }
		template<> CDescriptorHeapManager* GetHeapManager<D3D12_SHADER_RESOURCE_VIEW_DESC>() noexcept { return &SrvHeapManager; }
		template<> CDescriptorHeapManager* GetHeapManager<D3D12_UNORDERED_ACCESS_VIEW_DESC>() noexcept { return &UavHeapManager; }

		template<typename ViewDesc> D3D12DescriptorHeap* GetDescriptorHeap() noexcept;
		template<> D3D12DescriptorHeap* GetDescriptorHeap<D3D12_CONSTANT_BUFFER_VIEW_DESC>() noexcept { return nullptr; }
		template<> D3D12DescriptorHeap* GetDescriptorHeap<D3D12_SHADER_RESOURCE_VIEW_DESC>() noexcept { return &ResourceDescriptorHeap; }
		template<> D3D12DescriptorHeap* GetDescriptorHeap<D3D12_UNORDERED_ACCESS_VIEW_DESC>() noexcept { return &ResourceDescriptorHeap; }
		template<> D3D12DescriptorHeap* GetDescriptorHeap<D3D12_SAMPLER_DESC>() noexcept { return &SamplerDescriptorHeap; }
		// clang-format on
		[[nodiscard]] D3D12CommandContext&	   GetGraphicsContext() { return GraphicsContext; }
		[[nodiscard]] D3D12CommandContext&	   GetComputeContext() { return ComputeContext; }
		[[nodiscard]] D3D12CommandContext&	   GetCopyContext() { return CopyContext; }
		[[nodiscard]] D3D12ShaderResourceView* GetNullRaytracingAcceleraionStructure() { return &NullRaytracingAccelerationStructure; }

		void OnBeginFrame();
		void OnEndFrame();

		void WaitIdle();

		void			BeginResourceUpload();
		D3D12SyncHandle EndResourceUpload(bool WaitForCompletion);

		void Upload(const std::vector<D3D12_SUBRESOURCE_DATA>& Subresources, ID3D12Resource* Resource);
		void Upload(const D3D12_SUBRESOURCE_DATA& Subresource, ID3D12Resource* Resource);
		void Upload(const void* Data, UINT64 SizeInBytes, ID3D12Resource* Resource);

		void ReleaseDescriptor(DeferredDeleteDescriptor Descriptor);

	private:
		D3D12NodeMask	  NodeMask;
		D3D12CommandQueue GraphicsQueue;
		D3D12CommandQueue AsyncComputeQueue;
		D3D12CommandQueue CopyQueue;
		D3D12CommandQueue UploadQueue;
		D3D12Profiler	  Profiler;

		CDescriptorHeapManager RtvHeapManager;
		CDescriptorHeapManager DsvHeapManager;
		CDescriptorHeapManager SrvHeapManager;
		CDescriptorHeapManager UavHeapManager;
		D3D12DescriptorHeap	   ResourceDescriptorHeap;
		D3D12DescriptorHeap	   SamplerDescriptorHeap;

		D3D12CommandContext GraphicsContext;
		D3D12CommandContext ComputeContext;
		D3D12CommandContext CopyContext;
		D3D12CommandContext UploadContext;

		D3D12SyncHandle					 UploadSyncHandle;
		std::vector<Arc<ID3D12Resource>> TrackedResources;

		DeferredDeleteQueue<DeferredDeleteDescriptor> DeferredDeleteDescriptorQueue;

		// Null descriptors
		D3D12ShaderResourceView NullRaytracingAccelerationStructure;
	};

	template<typename ViewDesc, bool Dynamic>
	D3D12Descriptor<ViewDesc, Dynamic>::D3D12Descriptor(D3D12LinkedDevice* Parent)
		: D3D12LinkedDeviceChild(Parent)
	{
		if (Parent)
		{
			if constexpr (Dynamic)
			{
				D3D12DescriptorHeap* DescriptorHeap = Parent->GetDescriptorHeap<ViewDesc>();
				DescriptorHeap->Allocate(CpuHandle, GpuHandle, Index);
			}
			else if constexpr (!Dynamic)
			{
				CDescriptorHeapManager* Manager = Parent->GetHeapManager<ViewDesc>();
				CpuHandle						= Manager->AllocateHeapSlot(Index);
			}
		}
	}

	template<typename ViewDesc, bool Dynamic>
	void D3D12Descriptor<ViewDesc, Dynamic>::CreateDefaultView(ID3D12Resource* Resource)
	{
		(GetParentLinkedDevice()->GetDevice()->*Internal::D3D12DescriptorTraits<ViewDesc>::Create())(Resource, nullptr, CpuHandle);
	}

	template<typename ViewDesc, bool Dynamic>
	void D3D12Descriptor<ViewDesc, Dynamic>::CreateDefaultView(ID3D12Resource* Resource, ID3D12Resource* CounterResource)
	{
		(GetParentLinkedDevice()->GetDevice()->*Internal::D3D12DescriptorTraits<ViewDesc>::Create())(Resource, CounterResource, nullptr, CpuHandle);
	}

	template<typename ViewDesc, bool Dynamic>
	void D3D12Descriptor<ViewDesc, Dynamic>::CreateView(const ViewDesc& Desc, ID3D12Resource* Resource)
	{
		(GetParentLinkedDevice()->GetDevice()->*Internal::D3D12DescriptorTraits<ViewDesc>::Create())(Resource, &Desc, CpuHandle);
	}

	template<typename ViewDesc, bool Dynamic>
	void D3D12Descriptor<ViewDesc, Dynamic>::CreateView(const ViewDesc& Desc, ID3D12Resource* Resource, ID3D12Resource* CounterResource)
	{
		(GetParentLinkedDevice()->GetDevice()->*Internal::D3D12DescriptorTraits<ViewDesc>::Create())(Resource, CounterResource, &Desc, CpuHandle);
	}

	template<typename ViewDesc, bool Dynamic>
	void D3D12Descriptor<ViewDesc, Dynamic>::CreateSampler(const ViewDesc& Desc)
	{
		(GetParentLinkedDevice()->GetDevice()->*Internal::D3D12DescriptorTraits<ViewDesc>::Create())(&Desc, CpuHandle);
	}

	template<typename ViewDesc, bool Dynamic>
	void D3D12Descriptor<ViewDesc, Dynamic>::InternalDestroy()
	{
		if (Parent && IsValid())
		{
			if constexpr (Dynamic)
			{
				GetParentLinkedDevice()->ReleaseDescriptor(
					DeferredDeleteDescriptor{
						.OwningHeap = Parent->GetDescriptorHeap<ViewDesc>(),
						.Index		= Index,
					});
				Parent	  = nullptr;
				CpuHandle = { NULL };
				GpuHandle = { NULL };
				Index	  = UINT_MAX;
			}
			else if constexpr (!Dynamic)
			{
				CDescriptorHeapManager* Manager = Parent->GetHeapManager<ViewDesc>();
				Manager->FreeHeapSlot(CpuHandle, Index);
				Parent	  = nullptr;
				CpuHandle = { NULL };
				Index	  = UINT_MAX;
			}
		}
	}
} // namespace RHI
