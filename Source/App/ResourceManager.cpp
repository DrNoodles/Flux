
#include "ResourceManager.h"

#include "Renderer/VulkanHelpers.h"

#include <vector>
#include <memory>
#include <unordered_map>
#include <iostream>

using vkh = VulkanHelpers;


RenderableComponent ResourceManager::LoadRenderableComponentFromFile(const std::string& path)
{
	//std::vector<RenderableMesh> renderableMeshes{};

	auto modelDef = _modelLoaderService->LoadModel(path);
	if (!modelDef.has_value())
	{
		throw std::invalid_argument("Couldn't load model");
	}

	RenderableComponent rc = {};
	auto modelRes = std::make_unique<ModelResource>();
	
	for (const auto& meshDef : modelDef->Meshes)
	{
		// Create and store the Mesh Resource
		auto meshRes = CreateMeshResource(meshDef);
		modelRes->Mesh = meshRes.get();
		StoreMeshResource(std::move(meshRes));

		
		// Create and store the Texture Resources
		for (const auto& texDef : meshDef.Textures)
		{
			auto texRes = CreateTextureResource(texDef.Path);

			switch (texDef.Type)
			{
			case TextureType::BaseColor: modelRes->BasecolorMap = texRes.get(); break;
			case TextureType::Normals:   modelRes->NormalMap = texRes.get();    break;

			// Unhandled
			case TextureType::Undefined:
			case TextureType::Metalness:
			case TextureType::Roughness:
			case TextureType::AmbientOcclusion:
			default:;
				throw std::invalid_argument("Failed to load unhandled texture type");
			}

			StoreTextureResource(std::move(texRes));
		}

		// TODO Only processing the first mesh for now!
		break;
	}


	modelRes->Infos = CreateModelInfoResources(*modelRes);

	
	StoreModelResource(std::move(modelRes));

	
	//for (const auto& meshDef : modelDef->Meshes)
	//{
	//	RenderableMesh asset{};
	// asset.MeshId = StoreMeshResource(std::make_unique<MeshResource>(meshDef.Name, meshDef.Vertices, meshDef.Indices));

	//	for (const auto& texDef : meshDef.Textures)
	//	{

	//		
	//		// Load texture resource to GPU
	//		Texture texRes{};
	//		texRes.ResourceId = LoadTexture(texDef.Path);
	//		texRes.Type = texDef.Type;
	//		texRes.Path = texDef.Path;
	//		
	//		
	//		switch (texDef.Type)
	//		{
	//		case TextureType::BaseColor: 
	//			asset.Mat.UseAlbedoMap = true;
	//			asset.Mat.AlbedoMap = texRes;
	//			break;
	//			
	//		case TextureType::Normals:
	//			//asset.Mat.UseNormalMap = true;
	//			asset.Mat.NormalMap = texRes;
	//			break;

	//		case TextureType::AmbientOcclusion:
	//			//asset.Mat.UseAoMap = true;
	//			asset.Mat.AoMap = texRes;
	//			break;

	//		case TextureType::Roughness:
	//			asset.Mat.UseRoughnessMap = true;
	//			asset.Mat.RoughnessMap = texRes;
	//			break;

	//		case TextureType::Metalness:
	//			asset.Mat.UseMetallicMap = true;
	//			asset.Mat.MetalnessMap = texRes;
	//			break;

	//		case TextureType::Undefined:
	//		default:
	//			std::cerr << "Discarding unknown texture type" << std::endl;
	//		}
	//	}
	
	//	renderableMeshes.emplace_back(asset);
	//}

	//return Renderable{ renderableMeshes };
}

//
//u32 ResourceManager::LoadTexture(const std::string& path)
//{
//	// Is teh tex already loaded?
//	const auto it = _loadedTextures.find(path);
//	if (it != _loadedTextures.end())
//	{
//		return it->second;
//	}
//
//	// TODO safety check this actually exists :)
//	const size_t idx = path.find_last_of("/\\");
//	const auto directory = path.substr(0, idx + 1);
//	const auto filename = path.substr(idx + 1);
//
//	auto texRes = TextureLoader::LoadTextureFromFileAndPushToGpu(filename, directory, false);
//	auto id = StoreTextureResource(std::make_unique<TextureResource>(std::move(texRes)));
//
//	_loadedTextures.insert(std::make_pair(path, id));
//
//	return id;
//}



std::unique_ptr<TextureResource> ResourceManager::CreateTextureResource(const std::string& path) const
{
	auto texture = std::make_unique<TextureResource>();

	// TODO Pull the teture library out of the CreateTextureImage, just work on an TextureDefinition struct that
		// has an array of pixels and width, height, channels, etc
	
	std::tie(texture->Image, texture->Memory, texture->MipLevels, texture->Width, texture->Height)
		= vkh::CreateTextureImage(path, _commandPool, _graphicsQueue, _physicalDevice, _device);

	texture->View = vkh::CreateTextureImageView(texture->Image, texture->MipLevels, _device);

	texture->Sampler = vkh::CreateTextureSampler(texture->MipLevels, _device);

	return texture;
}
std::unique_ptr<MeshResource> ResourceManager::CreateMeshResource(const MeshDefinition& meshDefinition) const
{
	auto meshResources = std::make_unique<MeshResource>();

	// Compute AABB
	std::vector<glm::vec3> positions{ meshDefinition.Vertices.size() };
	for (size_t i = 0; i < meshDefinition.Vertices.size(); i++)
	{
		positions[i] = meshDefinition.Vertices[i].Pos;
	}
	const AABB bounds{ positions };


	// Load mesh resource
	meshResources->IndexCount = meshDefinition.Indices.size();
	meshResources->VertexCount = meshDefinition.Vertices.size();
	meshResources->Bounds = bounds;

	std::tie(meshResources->VertexBuffer, meshResources->VertexBufferMemory)
		= vkh::CreateVertexBuffer(meshDefinition.Vertices, _graphicsQueue, _commandPool, _physicalDevice, _device);

	std::tie(meshResources->IndexBuffer, meshResources->IndexBufferMemory)
		= vkh::CreateIndexBuffer(meshDefinition.Indices, _graphicsQueue, _commandPool, _physicalDevice, _device);

	return meshResources;
}
std::vector<ModelInfoResource> ResourceManager::CreateModelInfoResources(const ModelResource& model) const
{
	std::vector<ModelInfoResource> modelInfos{};

	const auto numThingsToMake = _swapchainImages.size();

	std::vector<VkBuffer> uniformBuffers;
	std::vector<VkDeviceMemory> uniformBuffersMemory;
	std::vector<VkDescriptorSet> descriptorSets;

	std::tie(uniformBuffers, uniformBuffersMemory) = vkh::CreateUniformBuffers(numThingsToMake, _device, _physicalDevice);

	descriptorSets = vkh::CreateDescriptorSets((uint32_t)numThingsToMake, _descriptorSetLayout, _descriptorPool,
		uniformBuffers, *model.BasecolorMap, *model.NormalMap, _device);


	modelInfos.resize(numThingsToMake);
	for (auto i = 0; i < numThingsToMake; i++)
	{
		modelInfos[i].UniformBuffer = uniformBuffers[i];
		modelInfos[i].UniformBufferMemory = uniformBuffersMemory[i];
		modelInfos[i].DescriptorSet = descriptorSets[i];
	}

	return modelInfos;
}


ModelResourceId ResourceManager::StoreModelResource(std::unique_ptr<ModelResource> model)
{
	_modelResources.emplace_back(std::move(model));
	return ModelResourceId(_modelResources.size() - 1);
}
u32 ResourceManager::StoreMeshResource(std::unique_ptr<MeshResource> mesh)
{
	_meshResources.emplace_back(std::move(mesh));
	return u32(_meshResources.size() - 1);
}
u32 ResourceManager::StoreTextureResource(std::unique_ptr<TextureResource> texture)
{
	_textureResources.emplace_back(std::move(texture));
	return u32(_textureResources.size() - 1);
}
