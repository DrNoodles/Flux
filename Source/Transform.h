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
	}

	glm::vec3 GetPos() const { return _position; }
	glm::vec3 GetRot() const { return _rotation; }
	glm::vec3 GetScale() const { return _scale; }
	void SetPos(const glm::vec3 pos) { _position = pos; }
	void SetRot(const glm::vec3 rot) { _rotation = rot; }
	void SetScale(const glm::vec3 scale) { _scale = scale; }

	glm::mat4 GetMatrix() const
	{
		glm::mat4 m{1};
		m = glm::translate(m, _position);
		m = glm::rotate(m, glm::radians(_rotation.x), {1,0,0});
		m = glm::rotate(m, glm::radians(_rotation.y), {0,1,0});
		m = glm::rotate(m, glm::radians(_rotation.z), {0,0,1});
		m = glm::scale(m, _scale);
		return m;
	}

private:
	glm::vec3 _position{};
	glm::vec3 _rotation{};
	glm::vec3 _scale{1};
};
