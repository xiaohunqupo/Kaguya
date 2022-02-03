#pragma once
#include "AssetImporter.h"

namespace Asset
{
	class AssetManager
	{
	public:
		AssetManager(RHI::D3D12Device* Device);
		~AssetManager();

		template<typename T, typename... TArgs>
		auto CreateAsset(TArgs&&... Args) -> typename AssetTypeTraits<T>::ApiType*
		{
			if constexpr (std::is_same_v<T, Mesh>)
			{
				AssetHandle Handle = MeshRegistry.Create(std::forward<TArgs>(Args)...);
				return MeshRegistry.GetAsset(Handle);
			}
			else if constexpr (std::is_same_v<T, Texture>)
			{
				AssetHandle Handle = TextureRegistry.Create(std::forward<TArgs>(Args)...);
				return TextureRegistry.GetAsset(Handle);
			}

			return nullptr;
		}

		AssetRegistry<Mesh>&	GetMeshRegistry() { return MeshRegistry; }
		AssetRegistry<Texture>& GetTextureRegistry() { return TextureRegistry; }

		AssetType GetAssetTypeFromExtension(const std::filesystem::path& Path);

		void AsyncLoadImage(const TextureImportOptions& Options);

		void AsyncLoadMesh(const MeshImportOptions& Options);

		void RequestUpload(Texture* Texture);

		void RequestUpload(Mesh* Mesh);

	private:
		void UploadTexture(Texture* AssetTexture, RHI::D3D12LinkedDevice* Device);
		void UploadMesh(Mesh* AssetMesh, RHI::D3D12LinkedDevice* Device);

	private:
		MeshImporter	MeshImporter;
		TextureImporter TextureImporter;

		AssetRegistry<Mesh>	   MeshRegistry;
		AssetRegistry<Texture> TextureRegistry;

		// Upload stuff to the GPU
		std::mutex				Mutex;
		std::condition_variable ConditionVariable;
		std::queue<Mesh*>		MeshUploadQueue;
		std::queue<Texture*>	TextureUploadQueue;

		std::jthread	  Thread;
		std::atomic<bool> Quit = false;

		friend class AssetWindow;
	};
} // namespace Asset
