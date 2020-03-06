#pragma once

#include <unordered_set>

class IblVm;
struct Entity;

class ISceneViewDelegate
{
public:
	virtual ~ISceneViewDelegate() = default;
	virtual void LoadDemoScene() = 0;
	virtual void LoadModel(const std::string& path) = 0;
	virtual void CreateLight() = 0;
	virtual void CreateSphere() = 0;
	virtual void CreateBlob() = 0;
	virtual void CreateCube() = 0;
	virtual void DeleteSelected() = 0;
	virtual void DeleteAll() = 0;
	virtual float GetExposure() const = 0;
	virtual void SetExposure(float exposure) = 0;
};

class ScenePane
{
public:
	ScenePane() = delete;
	explicit ScenePane(ISceneViewDelegate* delegate)
	{
		_delegate = delegate;
	}
	void DrawUI(const std::vector<Entity*>& ents, std::unordered_set<Entity*>& selection/*, IblVm& iblVm*/) const;

private:
	ISceneViewDelegate* _delegate = nullptr;
};
