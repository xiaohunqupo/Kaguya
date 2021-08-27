#include "D3D12Common.h"

LPCWSTR GetCommandQueueTypeString(ED3D12CommandQueueType CommandQueueType)
{
	switch (CommandQueueType)
	{
	case ED3D12CommandQueueType::Direct:
		return L"3D";
	case ED3D12CommandQueueType::AsyncCompute:
		return L"Async Compute";
	case ED3D12CommandQueueType::Copy1:
		return L"Copy 1";
	case ED3D12CommandQueueType::Copy2:
		return L"Copy 2";
	default:
		return nullptr;
	}
}

LPCWSTR GetCommandQueueTypeFenceString(ED3D12CommandQueueType CommandQueueType)
{
	switch (CommandQueueType)
	{
	case ED3D12CommandQueueType::Direct:
		return L"3D Fence";
	case ED3D12CommandQueueType::AsyncCompute:
		return L"Async Compute Fence";
	case ED3D12CommandQueueType::Copy1:
		return L"Copy 1 Fence";
	case ED3D12CommandQueueType::Copy2:
		return L"Copy 2 Fence";
	default:
		return nullptr;
	}
}

const char* D3D12Exception::GetErrorType() const noexcept
{
	return "[D3D12]";
}

std::string D3D12Exception::GetError() const
{
#define DXERR(x)                                                                                                       \
	case x:                                                                                                            \
		Error = #x;                                                                                                    \
		break

	std::string Error;
	// https://docs.microsoft.com/en-us/windows/win32/direct3d12/d3d12-graphics-reference-returnvalues
	switch (ErrorCode)
	{
		DXERR(D3D12_ERROR_ADAPTER_NOT_FOUND);
		DXERR(D3D12_ERROR_DRIVER_VERSION_MISMATCH);
		DXERR(DXGI_ERROR_INVALID_CALL);
		DXERR(DXGI_ERROR_WAS_STILL_DRAWING);
		DXERR(E_FAIL);
		DXERR(E_INVALIDARG);
		DXERR(E_OUTOFMEMORY);
		DXERR(E_NOTIMPL);
		DXERR(E_NOINTERFACE);

		// This just follows the documentation
		// DXGI
		DXERR(DXGI_ERROR_ACCESS_DENIED);
		DXERR(DXGI_ERROR_ACCESS_LOST);
		DXERR(DXGI_ERROR_ALREADY_EXISTS);
		DXERR(DXGI_ERROR_CANNOT_PROTECT_CONTENT);
		DXERR(DXGI_ERROR_DEVICE_HUNG);
		DXERR(DXGI_ERROR_DEVICE_REMOVED);
		DXERR(DXGI_ERROR_DEVICE_RESET);
		DXERR(DXGI_ERROR_DRIVER_INTERNAL_ERROR);
		DXERR(DXGI_ERROR_FRAME_STATISTICS_DISJOINT);
		DXERR(DXGI_ERROR_GRAPHICS_VIDPN_SOURCE_IN_USE);
		// DXERR(DXGI_ERROR_INVALID_CALL); // Already defined
		DXERR(DXGI_ERROR_MORE_DATA);
		DXERR(DXGI_ERROR_NAME_ALREADY_EXISTS);
		DXERR(DXGI_ERROR_NONEXCLUSIVE);
		DXERR(DXGI_ERROR_NOT_CURRENTLY_AVAILABLE);
		DXERR(DXGI_ERROR_NOT_FOUND);
		// DXERR(DXGI_ERROR_REMOTE_CLIENT_DISCONNECTED); // Reserved
		// DXERR(DXGI_ERROR_REMOTE_OUTOFMEMORY); // Reserved
		DXERR(DXGI_ERROR_RESTRICT_TO_OUTPUT_STALE);
		DXERR(DXGI_ERROR_SDK_COMPONENT_MISSING);
		DXERR(DXGI_ERROR_SESSION_DISCONNECTED);
		DXERR(DXGI_ERROR_UNSUPPORTED);
		DXERR(DXGI_ERROR_WAIT_TIMEOUT);
		// DXERR(DXGI_ERROR_WAS_STILL_DRAWING); // Already defined
	default:
	{
		char Buffer[64] = {};
		sprintf_s(Buffer, "HRESULT of 0x%08X", static_cast<UINT>(ErrorCode));
		Error = Buffer;
	}
	break;
	}
#undef DXERR
	return Error;
}

auto D3D12CommandSyncPoint::IsValid() const noexcept -> bool
{
	return Fence != nullptr;
}

auto D3D12CommandSyncPoint::GetValue() const noexcept -> UINT64
{
	assert(IsValid());
	return Value;
}

auto D3D12CommandSyncPoint::IsComplete() const -> bool
{
	assert(IsValid());
	return Fence->GetCompletedValue() >= Value;
}

auto D3D12CommandSyncPoint::WaitForCompletion() const -> void
{
	assert(IsValid());
	VERIFY_D3D12_API(Fence->SetEventOnCompletion(Value, nullptr));
}
