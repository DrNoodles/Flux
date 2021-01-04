#pragma once

#include <Framework/IModelLoaderService.h>
#include <Framework/Material.h>
#include <Framework/Vertex.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <vector>
#include <string>
#include <iostream>




class AssimpModelLoaderService final : public IModelLoaderService
{
public:
	std::optional<ModelDefinition> LoadModel(const std::string& path) override
	{
		ModelDefinition modelDefinition{};

		Assimp::Importer importer{};
		const aiScene *scene = importer.ReadFile(path, 
			aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace | aiProcess_GenBoundingBoxes 
			// | aiProcess_OptimizeMeshes | aiProcess_JoinIdenticalVertices  // TODO Experiment with more flags to optimise things
		);

		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
		{
			std::cerr << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
			return std::nullopt;
		}

		std::cout << "\nDumping: " << path << std::endl;
		DumpMaterialsToConsole(*scene);

		const auto directory = path.substr(0, path.find_last_of("/\\") + 1); // TODO Use FileService to split path
		ProcessNode(modelDefinition, scene->mRootNode, scene, directory);

		// TODO Process Materials independent to the meshes that use them. And add a materials list to the modelDefinition
		//ProcessMaterials(modelDefinition, scene);
		
		return modelDefinition;
	}

private:
	static void ProcessNode(ModelDefinition& outModel, aiNode *node,
		const aiScene *aiScene, const std::string& directory)
	{
		// Process all the meshes in this node
		for (unsigned int i = 0; i < node->mNumMeshes; ++i)
		{
			const u32 mId = node->mMeshes[i];
			aiMesh* aiMesh = aiScene->mMeshes[mId];
			MeshDefinition md = ProcessMesh(aiMesh, aiScene, directory);
			outModel.Meshes.emplace_back(std::move(md));
		}

		// Process all child nodes
		for (unsigned int i = 0; i < node->mNumChildren; ++i)
		{
			aiNode* child = node->mChildren[i];
			ProcessNode(outModel, child, aiScene, directory);
		}
	}
	
	static MeshDefinition ProcessMesh(aiMesh* mesh, const aiScene* aiScene,
		const std::string& directory)
	{
		MeshDefinition meshDefinition{};

		meshDefinition.MeshName = mesh->mName.C_Str();


		// Tally positions - for use in computing bounds
		std::vector<glm::vec3> positions{ mesh->mNumVertices };

		
		// Get verts
		for (uint32_t i = 0; i < mesh->mNumVertices; ++i)
		{
			Vertex v{};

			// get pos
			auto vert = mesh->mVertices[i];
			v.Pos = { vert.x,vert.y,vert.z };

			// get normal
			if (mesh->HasNormals())
			{
				auto norm = mesh->mNormals[i];
				v.Normal = { norm.x,norm.y,norm.z };
			}
			else
			{
				std::cout << "Failed to load mesh normals\n";
			}
			
			// get tangent
			if (mesh->HasTangentsAndBitangents())
			{
				auto tangent = mesh->mTangents[i];
				auto direction = 1; // TODO compute -1 if the normal direction is inverted
				v.Tangent = glm::vec4{ tangent.x, tangent.y, tangent.z, direction };
			}
			else
			{
				std::cout << "Failed to load mesh tangents\n";
			}
		
			
			// get uvs
			if (mesh->mTextureCoords[0] != nullptr)
			{
				auto uv = mesh->mTextureCoords[0][i];
				v.TexCoord = { uv.x, uv.y };
			}
			else
			{
				v.TexCoord = { 0,0 };
			}

			positions.push_back(v.Pos);
			meshDefinition.Vertices.push_back(v);
		}

		// Get indices
		for (uint32_t i = 0; i < mesh->mNumFaces; ++i)
		{
			const aiFace face = mesh->mFaces[i];
			for (uint32_t j = 0; j < face.mNumIndices; ++j)
			{
				uint32_t index = face.mIndices[j];
				meshDefinition.Indices.push_back(index);
			}
		}

		// Get materials
		if (mesh->mMaterialIndex >= 0)
		{
			aiMaterial* aiMat = aiScene->mMaterials[mesh->mMaterialIndex];

			meshDefinition.MaterialName = aiMat->GetName().C_Str();
			
			auto basecolor = LoadMaterialTextures(aiMat, aiTextureType_DIFFUSE, directory);
			auto normals = LoadMaterialTextures(aiMat, aiTextureType_NORMALS, directory);
			auto ao = LoadMaterialTextures(aiMat, aiTextureType_LIGHTMAP, directory);
			auto emissive = LoadMaterialTextures(aiMat, aiTextureType_EMISSIVE, directory);

			meshDefinition.Textures.insert(meshDefinition.Textures.end(), basecolor.begin(), basecolor.end());
			meshDefinition.Textures.insert(meshDefinition.Textures.end(), normals.begin(), normals.end());
			meshDefinition.Textures.insert(meshDefinition.Textures.end(), ao.begin(), ao.end());
			meshDefinition.Textures.insert(meshDefinition.Textures.end(), emissive.begin(), emissive.end());
		}

		// Compute AABB
		meshDefinition.Bounds = AABB{ positions };
		
		return meshDefinition;
	}
	
	static std::vector<TextureDefinition> LoadMaterialTextures(aiMaterial *mat, aiTextureType type,
		const std::string& directory)
	{
		std::vector<TextureDefinition> textures;

 		for (uint32_t i = 0; i < mat->GetTextureCount(type); ++i)
		{
			// get new texture
			aiString file;
			auto result = mat->GetTexture(type, i, &file);
 			
			TextureDefinition texture;
			texture.Type = MapTextureTypeFromAssimpToShared(type);
			texture.Path = directory + file.C_Str();

			textures.emplace_back(std::move(texture));
		}
		return textures;
	}
	
	static TextureType MapTextureTypeFromAssimpToShared(aiTextureType aiType)
	{
		switch (aiType)
		{
		case aiTextureType_DIFFUSE: return TextureType::Basecolor;
		case aiTextureType_EMISSIVE: return TextureType::Emissive;
		case aiTextureType_NORMALS: return TextureType::Normals;
		case aiTextureType_LIGHTMAP: return TextureType::AmbientOcclusion;
		
		case aiTextureType_NONE:
		case aiTextureType_SPECULAR:
		case aiTextureType_AMBIENT:
		case aiTextureType_HEIGHT:
		case aiTextureType_SHININESS:
		case aiTextureType_OPACITY:
		case aiTextureType_DISPLACEMENT:
		case aiTextureType_REFLECTION:
		case aiTextureType_BASE_COLOR:
		case aiTextureType_NORMAL_CAMERA:
		case aiTextureType_EMISSION_COLOR: 
		case aiTextureType_METALNESS:
		case aiTextureType_DIFFUSE_ROUGHNESS:
		case aiTextureType_AMBIENT_OCCLUSION:
		case aiTextureType_UNKNOWN:
		case _aiTextureType_Force32Bit:
		default:
			return TextureType::Undefined;
		}
	}
	
	static void DumpMaterialsToConsole(const aiScene& scene)
	{
		const auto toString = [](aiTextureType type)
		{
			switch (type)
			{
			case aiTextureType_NONE: return "aiNone";
			case aiTextureType_DIFFUSE: return "aiDiffuse";
			case aiTextureType_SPECULAR: return "aiSpecular";
			case aiTextureType_AMBIENT: return "aiAmbient";
			case aiTextureType_EMISSIVE: return "aiEmissive";
			case aiTextureType_HEIGHT: return "aiHeight";
			case aiTextureType_NORMALS: return "aiNormals";
			case aiTextureType_SHININESS: return "aiShininess";
			case aiTextureType_OPACITY: return "aiOpacity";
			case aiTextureType_DISPLACEMENT: return "aiDisplacement";
			case aiTextureType_LIGHTMAP: return "aiLightmap";
			case aiTextureType_REFLECTION: return "aiReflection";
			case aiTextureType_BASE_COLOR: return "aiBase_Color";
			case aiTextureType_NORMAL_CAMERA: return "aiNormal_Camera";
			case aiTextureType_EMISSION_COLOR: return "aiEmission_Color";
			case aiTextureType_METALNESS: return "aiMetalness";
			case aiTextureType_DIFFUSE_ROUGHNESS: return "aiDiffuse_Roughness";
			case aiTextureType_AMBIENT_OCCLUSION: return "aiAmbient_Occulusion";
			case aiTextureType_UNKNOWN: return "aiUnknown";
			default: return "BUG! Undefined Texture Type";
			}
		};
		const auto dumpTexType = [&toString](const aiMaterial& mat, aiTextureType type)
		{
			const unsigned count = mat.GetTextureCount(type);

			if (count == 0) 
				return;

			const auto texStr = count > 1 ? " textures:" : " texture:";
			std::cout << "  " /*<< count << "x "*/ << toString(type) << ":" << std::endl;
			for (unsigned i = 0; i < count; ++i)
			{
				aiString path;
				mat.GetTexture(type, i, &path);
				std::cout << "    " << path.C_Str() << std::endl;
			}
			
		};

		// Dump Textures
		for (unsigned i = 0; i < scene.mNumMaterials; i++)
		{
			const aiMaterial& mat = *scene.mMaterials[i];
			std::cout << "mat " << i << " has textures..." << std::endl;
			dumpTexType(mat, aiTextureType_NONE);				// 0
			dumpTexType(mat, aiTextureType_DIFFUSE);			// 1
			dumpTexType(mat, aiTextureType_SPECULAR);			// 2
			dumpTexType(mat, aiTextureType_AMBIENT);			// 3
			dumpTexType(mat, aiTextureType_EMISSIVE);			// 4
			dumpTexType(mat, aiTextureType_HEIGHT);			// 5
			dumpTexType(mat, aiTextureType_NORMALS);			// 6
			dumpTexType(mat, aiTextureType_SHININESS);		// 7
			dumpTexType(mat, aiTextureType_OPACITY);			// 8
			dumpTexType(mat, aiTextureType_DISPLACEMENT);	// 9
			dumpTexType(mat, aiTextureType_LIGHTMAP);			// 10
			dumpTexType(mat, aiTextureType_REFLECTION);		// 11
			dumpTexType(mat, aiTextureType_BASE_COLOR);		// 12
			dumpTexType(mat, aiTextureType_NORMAL_CAMERA);	// 13
			dumpTexType(mat, aiTextureType_EMISSION_COLOR);	// 14
			dumpTexType(mat, aiTextureType_METALNESS);		// 15
			dumpTexType(mat, aiTextureType_DIFFUSE_ROUGHNESS);//16
			dumpTexType(mat, aiTextureType_AMBIENT_OCCLUSION);//17
			dumpTexType(mat, aiTextureType_UNKNOWN);			// 18

			// Dump properties
		/*	for (unsigned j = 0; j < mat.mNumProperties; j++)
			{
				aiMaterialProperty& prop = *mat.mProperties[j];
				std::cout << "  prop:"<<prop.mKey.C_Str() << " : "<<prop.mType << std::endl;
			}*/
		}
	}
};
