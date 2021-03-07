#pragma once
#include <Framework/CommonRenderer.h>
#include <Framework/IModelLoaderService.h>


#include "IblLoader.h"
#include "Renderer/TextureResource.h"
#include "Renderer/VulkanService.h"

// The purpose of this class is to create/manage/destroy GPU textures and buffers resources.
class ResourceRegistry
{
public: // Data
private:// Data

	// Dependencies
	VulkanService* _vk;
	IModelLoaderService* _modelLoaderService = nullptr;;
	std::string _shaderDir;
	std::string _assetsDir;

	MeshResourceId _skyboxMeshId;
	
	std::vector<std::unique_ptr<MeshResource>> _meshes{};
	std::vector<std::unique_ptr<TextureResource>> _textures{};


public: // Lifetime
	ResourceRegistry() = delete;
	ResourceRegistry(VulkanService* vk, IModelLoaderService* modelLoader, std::string shaderDir, std::string assetsDir)
		: _vk(vk), _modelLoaderService(modelLoader), _shaderDir(std::move(shaderDir)), _assetsDir(std::move(assetsDir))
	{
		LoadHelperResources();
	}

	ResourceRegistry(const ResourceRegistry&) = delete;
	ResourceRegistry(ResourceRegistry&&) = delete;
	ResourceRegistry& operator=(const ResourceRegistry&) = delete;
	ResourceRegistry& operator=(ResourceRegistry&&) = delete;
	~ResourceRegistry()
	{
		// TODO Make all resources RAII
		for (auto& mesh : _meshes)  
		{
			vkDestroyBuffer(_vk->LogicalDevice(), mesh->IndexBuffer, nullptr);
			vkFreeMemory(_vk->LogicalDevice(), mesh->IndexBufferMemory, nullptr);
			vkDestroyBuffer(_vk->LogicalDevice(), mesh->VertexBuffer, nullptr);
			vkFreeMemory(_vk->LogicalDevice(), mesh->VertexBufferMemory, nullptr);
		}
		
		_textures.clear(); // RAII will cleanup
	}

public: // Methods
	
	const TextureResource& GetTexture(TextureResourceId id) const { return *_textures[id.Value()]; }
	const MeshResource& GetMesh(MeshResourceId id) const { return *_meshes[id.Value()]; }
	const std::vector<std::unique_ptr<MeshResource>>& Hack_GetMeshes() const { return _meshes; }

	TextureResourceId CreateTextureResource(const std::string& path)
	{
		const auto id = TextureResourceId(static_cast<u32>(_textures.size()));
		auto texRes = TextureResourceHelpers::LoadTexture(path, _vk->CommandPool(), _vk->GraphicsQueue(), _vk->PhysicalDevice(), _vk->LogicalDevice());
		_textures.emplace_back(std::make_unique<TextureResource>(std::move(texRes)));
		return id;
	}

	IblTextureResourceIds CreateIblTextureResources(const std::array<std::string, 6>& sidePaths)
	{
		IblTextureResources iblRes = IblLoader::LoadIblFromCubemapPath(sidePaths, GetMesh(_skyboxMeshId), _shaderDir, 
			_vk->CommandPool(), _vk->GraphicsQueue(), _vk->PhysicalDevice(), _vk->LogicalDevice());

		IblTextureResourceIds ids = {};

		ids.EnvironmentCubemapId = TextureResourceId(static_cast<u32>(_textures.size()));
		_textures.emplace_back(std::make_unique<TextureResource>(std::move(iblRes.EnvironmentCubemap)));
			
		ids.IrradianceCubemapId = TextureResourceId(static_cast<u32>(_textures.size()));
		_textures.emplace_back(std::make_unique<TextureResource>(std::move(iblRes.IrradianceCubemap)));

		ids.PrefilterCubemapId = TextureResourceId(static_cast<u32>(_textures.size()));
		_textures.emplace_back(std::make_unique<TextureResource>(std::move(iblRes.PrefilterCubemap)));

		ids.BrdfLutId = TextureResourceId(static_cast<u32>(_textures.size()));
		_textures.emplace_back(std::make_unique<TextureResource>(std::move(iblRes.BrdfLut)));

		return ids;
	}

	IblTextureResourceIds CreateIblTextureResources(const std::string& path)
	{
		IblTextureResources iblRes = IblLoader::LoadIblFromEquirectangularPath(path, GetMesh(_skyboxMeshId), _shaderDir,
			_vk->CommandPool(), _vk->GraphicsQueue(), _vk->PhysicalDevice(), _vk->LogicalDevice());

		IblTextureResourceIds ids = {};

		ids.EnvironmentCubemapId = TextureResourceId(static_cast<u32>(_textures.size()));
		_textures.emplace_back(std::make_unique<TextureResource>(std::move(iblRes.EnvironmentCubemap)));

		ids.IrradianceCubemapId = TextureResourceId(static_cast<u32>(_textures.size()));
		_textures.emplace_back(std::make_unique<TextureResource>(std::move(iblRes.IrradianceCubemap)));

		ids.PrefilterCubemapId = TextureResourceId(static_cast<u32>(_textures.size()));
		_textures.emplace_back(std::make_unique<TextureResource>(std::move(iblRes.PrefilterCubemap)));

		ids.BrdfLutId = TextureResourceId(static_cast<u32>(_textures.size()));
		_textures.emplace_back(std::make_unique<TextureResource>(std::move(iblRes.BrdfLut)));

		return ids;
	}

	TextureResourceId CreateCubemapTextureResource(const std::array<std::string, 6>& sidePaths, CubemapFormat format)
	{
		const auto id = TextureResourceId(static_cast<u32>(_textures.size()));

		_textures.emplace_back(std::make_unique<TextureResource>(
			CubemapTextureLoader::LoadFromFacePaths(
				sidePaths, format, _vk->CommandPool(), _vk->GraphicsQueue(), _vk->PhysicalDevice(), _vk->LogicalDevice())));

		return id;
	}
	
	MeshResourceId CreateMeshResource(const MeshDefinition& meshDefinition)
	{
		// Load mesh resource
		auto mesh = std::make_unique<MeshResource>();

		mesh->IndexCount = meshDefinition.Indices.size();
		mesh->VertexCount = meshDefinition.Vertices.size();
		//mesh->Bounds = meshDefinition.Bounds;

		std::tie(mesh->VertexBuffer, mesh->VertexBufferMemory)
			= vkh::CreateVertexBuffer(meshDefinition.Vertices, _vk->GraphicsQueue(), _vk->CommandPool(), _vk->PhysicalDevice(), _vk->LogicalDevice());

		std::tie(mesh->IndexBuffer, mesh->IndexBufferMemory)
			= vkh::CreateIndexBuffer(meshDefinition.Indices, _vk->GraphicsQueue(), _vk->CommandPool(), _vk->PhysicalDevice(), _vk->LogicalDevice());


		const auto id = MeshResourceId(static_cast<u32>(_meshes.size()));
		_meshes.emplace_back(std::move(mesh));

		return id;
	}

private:// Methods
	void LoadHelperResources()
	{
		// Load a skybox mesh
		auto model = _modelLoaderService->LoadModel(_assetsDir + "skybox.obj");
		auto& meshDefinition = model.value().Meshes[0];
		_skyboxMeshId = CreateMeshResource(meshDefinition);
	}
	
};