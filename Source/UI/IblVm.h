#pragma once

#include <App/SceneManager.h>

#include <cassert>

class IblVm final
{
public:
	
	//std::vector<std::string> Ibls{};
	//int ActiveIbl = 0;
	int Rotation = 0;
	bool ShowIrradiance = false;

	IblVm() = default;
	IblVm(SceneManager* sc, RenderOptions* ro) : _sc{ sc }, _ro{ ro }
	{
		assert(sc);
		assert(ro);
		Update();
	}

	void Update()
	{
		//auto& cubemaps = _sc->CubemapsView();

		//Ibls.clear();
		//Ibls.reserve(cubemaps.size());
		//for (auto&& i : cubemaps)
		//{
		//	Ibls.emplace_back(i->Name());
		//}

		//ActiveIbl = _sc->GetActiveCubemap();
		
		Rotation = (i32)_ro->SkyboxRotation;
		ShowIrradiance = _ro->ShowIrradiance;
	}
	void Commit()
	{
		Rotation %= 360; // ensure (-360,360) range
		if (Rotation < 0) // ensure [0,360] range
			Rotation += 360; 
		
		_ro->SkyboxRotation = (f32)Rotation;
		_ro->ShowIrradiance = ShowIrradiance;
		//_sc->SetActiveCubemap(ActiveIbl);
		//_sc->CubemapsView()[ActiveIbl]->SetRotation(Rotation);
	}
	
private:
	SceneManager* _sc = nullptr;
	RenderOptions* _ro = nullptr;
};
