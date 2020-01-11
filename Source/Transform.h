#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Transform
{
public:
	Transform() = default;
	Transform(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale)
	{
		_position = pos;
		_rotation = rot;
		_scale = scale;
		_dirty = true;
	}

	glm::vec3 GetPos() const { return _position; }
	glm::vec3 GetRot() const { return _rotation; }
	glm::vec3 GetScale() const { return _scale; }
	void SetPos(const glm::vec3 pos) { _position = pos; _dirty = true; }
	void SetRot(const glm::vec3 rot) { _rotation = rot; _dirty = true; }
	void SetScale(const glm::vec3 scale) { _scale = scale; _dirty = true; }

	const glm::mat4& GetMatrix()
	{
		if (_dirty)
		{
			// This is slow TODO introduce _dirty flag? Only recompute if dirty?
			glm::mat4 m{ 1 };
			m = glm::translate(m, _position);
			m = glm::rotate(m, glm::radians(_rotation.x), { 1,0,0 });
			m = glm::rotate(m, glm::radians(_rotation.y), { 0,1,0 });
			m = glm::rotate(m, glm::radians(_rotation.z), { 0,0,1 });
			m = glm::scale(m, _scale);
			_mat = m;
			_dirty = false;
		}
		
		return _mat;
	}

private:
	glm::vec3 _position{};
	glm::vec3 _rotation{};
	glm::vec3 _scale{1};
	glm::mat4 _mat{ 1 };
	bool _dirty = false;
};
