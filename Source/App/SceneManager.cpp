
#include "SceneManager.h"

#include "Renderer/VulkanHelpers.h"

#include <vector>
#include <memory>
#include <unordered_map>
#include <iostream>

using vkh = VulkanHelpers;


RenderableComponent SceneManager::LoadRenderableComponentFromFile(const std::string& path)
{
	auto modelDefinition = _modelLoaderService.LoadModel(path);
	if (!modelDefinition.has_value())
	{
		throw std::invalid_argument("Couldn't load model");
	}

	RenderableComponent rc = {};
	CreateModelResourceInfo createModelResourceInfo = {};

	for (const auto& meshDef : modelDefinition->Meshes)
	{
		// Create the Mesh resource
		createModelResourceInfo.Mesh = _renderer.CreateMeshResource(meshDef);


		// Create Texture resources
		for (const auto& texDef : meshDef.Textures)
		{
			const auto texResId = _renderer.CreateTextureResource(texDef.Path);

			switch (texDef.Type)
			{
			case TextureType::BaseColor: createModelResourceInfo.BasecolorMap = texResId; break;
			case TextureType::Normals:   createModelResourceInfo.NormalMap    = texResId; break;

				// Unhandled
			case TextureType::Undefined:
			case TextureType::Metalness:
			case TextureType::Roughness:
			case TextureType::AmbientOcclusion:
				continue;
			default:;
				throw std::invalid_argument("Failed to load unhandled texture type");
			}
		}

		rc.ModelResId = _renderer.CreateModelResource(createModelResourceInfo);

		// TODO Only processing the first mesh for now!
		break;
	}

	return rc;
}
	
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
//}

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



//
//ModelResourceId ResourceManager::StoreModelResource(std::unique_ptr<ModelResource> model)
//{
//	_modelResources.emplace_back(std::move(model));
//	return ModelResourceId(_modelResources.size() - 1);
//}
//MeshResourceId ResourceManager::StoreMeshResource(std::unique_ptr<MeshResource> mesh)
//{
//	_meshResources.emplace_back(std::move(mesh));
//	return MeshResourceId(_meshResources.size() - 1);
//}
//TextureResourceId ResourceManager::StoreTextureResource(std::unique_ptr<TextureResource> texture)
//{
//	_textureResources.emplace_back(std::move(texture));
//	return TextureResourceId(_textureResources.size() - 1);
//}
