#pragma once

#include <Framework/CommonTypes.h>

#include <unordered_set>


struct SkyboxInfo;
struct RenderOptions;
struct Entity;
class IblVm;

class ISceneViewDelegate
{
public:
	virtual ~ISceneViewDelegate() = default;
	virtual void LoadDemoScene() = 0;
	virtual void LoadHeavyDemoScene() = 0;
	virtual void LoadModel(const std::string& path) = 0;
	virtual void CreateDirectionalLight() = 0;
	virtual void CreatePointLight() = 0;
	virtual void CreateSphere() = 0;
	virtual void CreateBlob() = 0;
	virtual void CreateCube() = 0;
	virtual void DeleteSelected() = 0;
	virtual void DeleteAll() = 0;
	
	virtual const RenderOptions& GetRenderOptions() = 0;
	virtual void SetRenderOptions(const RenderOptions& ro) = 0;
	
	virtual float GetSkyboxRotation() const = 0;
	virtual void SetSkyboxRotation(float skyboxRotation) = 0;
	
	virtual const std::vector<SkyboxInfo>& GetSkyboxList() = 0;
	virtual u32 GetActiveSkybox() const = 0;
	virtual void SetActiveSkybox(u32 idx) = 0;
};

class SceneView
{
public:
	SceneView() = delete;
	explicit SceneView(ISceneViewDelegate* delegate) : _delegate{ delegate } {}
	void BuildUI(const std::vector<Entity*>& ents, std::unordered_set<Entity*>& selection, IblVm& iblVm) const;

private:
	ISceneViewDelegate* _delegate = nullptr;
};
