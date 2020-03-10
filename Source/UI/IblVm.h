#pragma once

#include <App/SceneManager.h>

#include <cassert>

class IblVm final
{
public:
	
	//std::vector<std::string> Ibls{};
	//int ActiveIbl = 0;
	int Rotation = 0;
	//bool ShowIrradiance = false;

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
		Rotation = _ro->SkyboxRotation;
		
		//ShowIrradiance = _ro->ShowIrradiance;
	}
	void Commit()
	{
		if (Rotation >= 360)
			Rotation -= 360;

		if (Rotation < 0)
			Rotation += 360;
		
		_ro->SkyboxRotation = (float)Rotation;
		/*_sc->SetActiveCubemap(ActiveIbl);
		_sc->CubemapsView()[ActiveIbl]->SetRotation(Rotation);
		_ro->ShowIrradiance = ShowIrradiance;*/
	}
	
private:
	SceneManager* _sc = nullptr;
	RenderOptions* _ro = nullptr;
};
