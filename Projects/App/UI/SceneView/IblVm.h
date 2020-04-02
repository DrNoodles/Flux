#pragma once

#include <State/LibraryManager.h> //SkyboxInfo
#include <Framework/CommonRenderer.h> //RenderOptions

#include <cassert>


// TODO Get rid of this - lets just go with the _delegate system to remove ViewModels entirely
class IblVm final
{
public:
	int Rotation = 0;
	bool ShowIrradiance = false;

	IblVm() = default;
	IblVm(RenderOptions* ro) : _ro{ ro }
	{
		assert(ro);
		Update();
	}

	void Update()
	{
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
	}
	
private:
	RenderOptions* _ro = nullptr;
};
