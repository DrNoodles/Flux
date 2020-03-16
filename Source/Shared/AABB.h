#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <vector>

struct AABB
{
	AABB() = default;
	AABB(const glm::vec3& min, const glm::vec3& max)
	{
		_min = glm::min(min, max);
		_max = glm::max(min, max);
	}
	explicit AABB(const std::vector<glm::vec3>& points)
	{
		assert(!points.empty());
		
		// Compute Bounds
		_min = points[0];
		_max = points[0];
		for (auto& p : points)
		{
			if (p.x < _min.x) _min.x = p.x;
			if (p.y < _min.y) _min.y = p.y;
			if (p.z < _min.z) _min.z = p.z;
			if (p.x > _max.x) _max.x = p.x;
			if (p.y > _max.y) _max.y = p.y;
			if (p.z > _max.z) _max.z = p.z;
		}
	}
	
	glm::vec3 Min() const { return _min; }
	glm::vec3 Max() const { return _max; }
	std::vector<glm::vec3> Corners() const
	{
		return std::vector<glm::vec3>
		{
			glm::vec3{ _min.x, _min.y, _min.z },
			glm::vec3{ _max.x, _min.y, _min.z },
			glm::vec3{ _min.x, _max.y, _min.z },
			glm::vec3{ _max.x, _max.y, _min.z },
			glm::vec3{ _min.x, _min.y, _max.z },
			glm::vec3{ _max.x, _min.y, _max.z },
			glm::vec3{ _min.x, _max.y, _max.z },
			glm::vec3{ _max.x, _max.y, _max.z },
		};
	}

	float Volume() const
	{
		return (_max.x - _min.x) * (_max.y - _min.y) * (_max.z - _min.z);
	}
	
	/*AABB Transform(const glm::mat3& transform) const
	{
		return AABB{
			transform * _min,
			transform * _max
		};
	}*/

	// TODO change this to create an oriented bounding box. The user can then create a bounding box from the OBB.
	AABB Transform(const glm::mat4& transform) const
	{
		// Transform all corners of the AABB then create a new AABB from the transformed corners.
		
		auto corners = Corners();
		std::vector<glm::vec3> cornersNew{};
		for (auto& c : corners)
		{
			auto cn = glm::vec3{ transform * glm::vec4{ c, 1.f } };
			cornersNew.emplace_back(cn);
		}
		
		return AABB{ cornersNew };
	}
	glm::vec3 Center() const { return (_min + _max) / 2.f; }

	AABB Merge(const AABB& other) const
	{
		return Merge(*this, other);
	}

	bool IsEmpty() const 
	{
		return _max == _min;
	}

	static AABB Merge(const AABB& a, const AABB& b)
	{
		const glm::vec3 small{
			fmin(a.Min().x, b.Min().x),
			fmin(a.Min().y, b.Min().y),
			fmin(a.Min().x, b.Min().z) };
		const glm::vec3  big{
			fmax(a.Max().x, b.Max().x),
			fmax(a.Max().y, b.Max().y),
			fmax(a.Max().x, b.Max().z) };
		return AABB{ small, big };
	}
	
private:
	glm::vec3 _min{};
	glm::vec3 _max{};
};
