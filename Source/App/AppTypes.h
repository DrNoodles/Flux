#pragma once

#include "../Shared/CommonTypes.h"
#include "../Shared/Transform.h"
#include <string>
#include <vector>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct AppOptions
{
	std::string ShaderDir{};
	std::string AssetsDir{};
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct RenderableComponent
{
	u32 ModelId = u32_max;

};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct Entity
{
	RenderableComponent Renderable;
	Transform Transform;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct RenderableMesh
{
	u32 MeshId;
	u32 MatId;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FpsCounter
{
public:
	FpsCounter() : FpsCounter(120) {}
	explicit FpsCounter(size_t bufferSize)
	{
		_buffer.resize(bufferSize, 0);
	}
	void AddFrameTime(double dt)
	{
		_secPerFrameAccumulator += dt - _buffer[_index];
		_buffer[_index] = dt;
		_index = (_index + 1) % _buffer.size();
	}

	double GetFps() const
	{
		return _buffer.size() / _secPerFrameAccumulator;
	}

private:
	double _secPerFrameAccumulator = 0;
	std::vector<double> _buffer{};
	size_t _index = 0;
};
