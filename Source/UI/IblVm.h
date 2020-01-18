#pragma once
#include "App/SceneManager.h"

#include <cassert>
//
//class IblVm final
//{
//public:
//	
//	std::vector<std::string> Ibls{};
//	int ActiveIbl = 0;
//	float Rotation = 0;
//	bool ShowIrradiance = false;
//
//	IblVm() = default;
//	IblVm(SceneManager* sc, RenderOptions* ro) : _sc{ sc }, _ro{ ro }
//	{
//		assert(sc);
//		assert(ro);
//		Update();
//	}
//
//	void Update()
//	{
//		auto& cubemaps = _sc->CubemapsView();
//
//		Ibls.clear();
//		Ibls.reserve(cubemaps.size());
//		for (auto&& i : cubemaps)
//		{
//			Ibls.emplace_back(i->Name());
//		}
//
//		ActiveIbl = _sc->GetActiveCubemap();
//		Rotation = cubemaps[ActiveIbl]->Rotation();
//		
//		ShowIrradiance = _ro->ShowIrradiance;
//	}
//	void Commit() const
//	{
//		_sc->SetActiveCubemap(ActiveIbl);
//		_sc->CubemapsView()[ActiveIbl]->SetRotation(Rotation);
//		_ro->ShowIrradiance = ShowIrradiance;
//	}
//	
//private:
//	SceneController* _sc = nullptr;
//	RenderOptions* _ro = nullptr;
//};
