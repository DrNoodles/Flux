#pragma once

#include <imgui/imgui.h>
#include <Framework/CommonTypes.h>

#include <unordered_set>



struct SkyboxInfo;
struct RenderOptions;
struct Entity;
class IblVm;
typedef int ImGuiTreeNodeFlags;

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
	
	virtual RenderOptions GetRenderOptions() = 0;
	virtual void SetRenderOptions(const RenderOptions& ro) = 0;

	virtual void LoadAndSetSkybox() = 0;
	virtual const std::vector<SkyboxInfo>& GetSkyboxList() = 0;
	virtual u32 GetActiveSkybox() const = 0;
	virtual void SetActiveSkybox(u32 idx) = 0;
};

class SceneView
{
public:
private:
	ISceneViewDelegate* _del = nullptr;

	// TODO refactor to have some control over styling - This is copied in SceneView.h
	ImVec4 _headingColor = ImVec4{ .5,.5,.5,1 };
	
public:
	SceneView() = delete;
	explicit SceneView(ISceneViewDelegate* delegate) : _del{ delegate } {}
	void BuildUI(const std::vector<Entity*>& ents, std::unordered_set<Entity*>& selection) const;
	
private:
	void SceneLoadPanel(ImGuiTreeNodeFlags headerFlags) const;
	void CreationPanel(ImGuiTreeNodeFlags headerFlags) const;
	void OutlinerPanel(const std::vector<Entity*>& ents, std::unordered_set<Entity*>& selection, ImGuiTreeNodeFlags headerFlags) const;
	void IblPanel(ImGuiTreeNodeFlags headerFlags) const;
	void BackdropPanel(ImGuiTreeNodeFlags headerFlags) const;
	void CameraPanel(ImGuiTreeNodeFlags headerFlags) const;
	
	void PostPanel(ImGuiTreeNodeFlags headerFlags) const;
	void PostVignette() const;
	void PostGrain() const;
};
