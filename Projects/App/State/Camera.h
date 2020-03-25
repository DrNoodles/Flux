#pragma once

#include <Framework/AABB.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <iostream>

// Defines several possible options for camera movement. Used as abstraction to stay away from window-system specific input methods
enum class Speed {
	Normal,
	Fast,
	Slow,
};

// Camera defaults looks along +Z with +Y is up.
// TODO Camera is special case and receives input from mouse/keyboard without using the component system. Make consistent?
class Camera
{
public:
	float MovementSpeed = 40.0f;
	float MouseSensitivity = 0.004f;
	float Zoom = 45.0f; // vertical fov in degrees. From target direction to top of frame
	
	// Camera Attributes - todo put behind getters
	glm::vec3 Right	 = glm::vec3{ 1, 0, 0 }; // x - Right when looking toward the cam, NOT from the cam pov
	glm::vec3 Up		 = glm::vec3{ 0, 1, 0 }; // y
	glm::vec3 Forward  = glm::vec3{ 0, 0, 1 }; // z - z+ looks at the target
	
	glm::vec3 Position = glm::vec3{ 0, 2, 5 };
	glm::vec3 Target   = glm::vec3{ 0 }; // todo change camera to always have a target

	// Constructor with vectors
	Camera()
	{
		Forward = glm::normalize(Target - Position);
		Right = glm::normalize(glm::cross(_worldUp, Forward));
		Up = glm::normalize(glm::cross(Forward, Right));
	}
	Camera(glm::vec3 pos, glm::vec3 target) : Camera()
	{
		Position = pos;
		Target = target;
	}
	
	// Returns the view matrix calculated using Euler Angles and the LookAt Matrix
	glm::mat4 GetViewMatrix() const
	{
		/*std::cout
			<< "Pos(" << Position.x << Position.y << Position.z << ")"
			<< "Target(" << Target.x << Target.y << Target.z << ")"
			<< "_worldUp(" << _worldUp.x << _worldUp.y << _worldUp.z << ")"
			<< std::endl;*/
		
		return glm::lookAt(Position, Target, _worldUp);
	}
	
	// Processes input received from a mouse input system. Expects the offset value in both the x and y direction.
	//void ProcessMouseMovement(float xoffset, float yoffset, GLboolean constrainPitch = true)
	//{
	//	xoffset *= MouseSensitivity;
	//	yoffset *= MouseSensitivity;

	//	// TODO Pitch flip protection
	//	// Compute and store pitch from Forward
	//	// Compute and store pitch from Forward after rotate
	//	// If flipped, disard new forward, else, use it.

	//	Forward = glm::rotate(Forward, -yoffset, Right); // pitch
	//	Forward = glm::rotate(Forward, -xoffset, _worldUp); // yaw (must be after pitch as yaw dirties Right)

	//	Right = glm::normalize(glm::cross(_worldUp, Forward));
	//	Up = glm::normalize(glm::cross(Forward, Right));
	//}

	// Processes input received from a mouse scroll-wheel event. Only requires input on the vertical wheel-axis
	void ProcessMouseScroll(float yOffset)
	{
		const auto zoomSensitivity = 4.0f;
		const auto z = yOffset * zoomSensitivity;
		Zoom = glm::clamp(Zoom -= z, 1.0f, 45.0f);
	}

	void Focus(glm::vec3 point)
	{
		const auto displacement = Position - Target;
		Target = point;
		Position = point + displacement;
		// NOTE: No need to recompute axis as they're positionless
	}

	void Focus(glm::vec3 centerwlrd, float radius, float viewportAspect)
	{
		assert(radius > 0);
		// The goal is to find a rectangle that surrounds the volume from the camera's point of view.
		// The center of the rectangle will be the new Target
		// For the Position, we find the top middle of the rectangle and then using tan(angle) = o/a where o=len(top-center) and angle=Zoom/2. We solve for 'a' to give us the distance away from the object we need to move the camera
		// We repeat the same for the horizontal and pick the best fit

		auto camMat = GetViewMatrix();

		
		glm::vec3 topCamSpc, sideCamSpc;


		auto centerCamSpc = glm::vec3{ inverse(camMat) * glm::vec4{ centerwlrd,1 } };
		auto radiusVecCamSpc = glm::vec3{ inverse(camMat) * glm::vec4{ radius,0,0,0 } };
		auto radiusCamSpc = glm::length(radiusVecCamSpc);

		topCamSpc = centerCamSpc + radiusCamSpc * glm::vec3{ 0,1,0 };
		sideCamSpc = centerCamSpc + radiusCamSpc * glm::vec3{ 1,0,0 };

		auto midWldSpc = glm::vec3{ camMat * glm::vec4{centerCamSpc,1} };
		auto topWldSpc = glm::vec3{ camMat * glm::vec4{topCamSpc,1} };
		auto sideWldSpc = glm::vec3{ camMat * glm::vec4{sideCamSpc,1} };

		//printf_s("mid:%.3f,%.3f,%.3f  top:%.3f,%.3f,%.3f  vol:%.3f \n", midWldSpc.x, midWldSpc.y, midWldSpc.z, topWldSpc.x, topWldSpc.y, topWldSpc.z, boundsCamAligned.Volume());

		// Using tan(theta) = o/a to compute a. Where theta = radians(Zoom/2) and o = len(top-mid).
		auto hDistWldSpc = glm::length(topWldSpc - midWldSpc) / tan(glm::radians(Zoom * 0.5f));
		auto vDistWldSpc = glm::length(sideWldSpc - midWldSpc) / tan(glm::radians(viewportAspect * Zoom * 0.5f));

		auto dist = glm::max(hDistWldSpc, vDistWldSpc);

		const auto camDirection = glm::normalize(Position - Target);
		Target = midWldSpc;
		Position = midWldSpc + camDirection * dist * 0.5f; // magic # to adjust zoom a bit

		// NOTE: No need to recompute axis as they're positionless 
	}
	void Focus(AABB volume, float viewportAspect)
	{
		// The goal is to find a rectangle that surrounds the volume from the camera's point of view.
		// The center of the rectangle will be the new Target
		// For the Position, we find the top middle of the rectangle and then using tan(angle) = o/a where o=len(top-center) and angle=Zoom/2. We solve for 'a' to give us the distance away from the object we need to move the camera
		// We repeat the same for the horizontal and pick the best fit

		
		auto camMat = GetViewMatrix();
		
		//// Create a new bounds aligned with the camera that surround the world space volume.
		auto volCamSpace = volume.Transform(inverse(camMat));


		//// Get points focal points of interest on of the new camera space bounds.
		glm::vec3 mid = volCamSpace.Center();
		glm::vec3 top = volCamSpace.Center();
		top.y = volCamSpace.Max().y;
		glm::vec3 side = volCamSpace.Center();
		side.x = volCamSpace.Max().x;
	
		auto midWldSpc = glm::vec3{ camMat * glm::vec4{mid,1} };
		auto topWldSpc = glm::vec3{ camMat * glm::vec4{top,1} };
		auto sideWldSpc = glm::vec3{ camMat * glm::vec4{side,1} };

		//printf_s("mid:%.3f,%.3f,%.3f  top:%.3f,%.3f,%.3f  vol:%.3f \n", midWldSpc.x, midWldSpc.y, midWldSpc.z, topWldSpc.x, topWldSpc.y, topWldSpc.z, boundsCamAligned.Volume());
		
		// Using tan(theta) = o/a to compute a. Where theta = radians(Zoom/2) and o = len(top-mid).
		auto hDistWldSpc = glm::length(topWldSpc - midWldSpc) / tan(glm::radians(Zoom / 2));
		auto vDistWldSpc = glm::length(sideWldSpc - midWldSpc) / tan(glm::radians((viewportAspect * Zoom) / 2));

		auto dist = glm::max(hDistWldSpc, vDistWldSpc);
		
		const auto camDirection = glm::normalize(Position - Target);
		Target = midWldSpc;
		Position = midWldSpc + camDirection * dist * 1.8f; // magic # to zoom out a bit more

		// NOTE: No need to recompute axis as they're positionless 
	}
	
	void Move(float multiplier, const glm::vec3& direction, Speed speed)
	{
		switch (speed)
		{
		case Speed::Normal: multiplier *= 1.f; break;
		case Speed::Fast:	multiplier *= 4.f; break;
		case Speed::Slow: multiplier *= .25f; break;
		}

		const auto velocity = multiplier * MovementSpeed * direction;

		const auto newPos = Position + (Right * velocity.x + Up * velocity.y + Forward * velocity.z);
		const auto newTarget = Target + (Right * velocity.x + Up * velocity.y);

		// Make sure we're not zooming through to the other side
		const auto oldDir = glm::normalize(Target - Position);
		const auto newDir = glm::normalize(newTarget - newPos);
		if (dot(oldDir, newDir) <= 0)
		{
			return;
		}
		
		Position = newPos;
		Target = newTarget; // Don't offset target on depth axis
	}
	
	// radians
	void Arc(float yaw, float pitch)
	{
		const auto camDisplacement = Position - Target;
		const auto camDir = glm::normalize(camDisplacement);

		auto yawMat = glm::rotate(yaw, _worldUp);
		auto pitchMat = glm::rotate(pitch, Right);
		auto newCamDir = glm::vec3{ glm::vec4{ camDir,0 } * yawMat * pitchMat };

		
		auto newPosition = Target + newCamDir * glm::length(camDisplacement);
		auto newForward = glm::normalize(Target - newPosition);
		auto newRight = glm::normalize(glm::cross(_worldUp, newForward));
		auto newUp = glm::normalize(glm::cross(newForward, newRight));

		// Protect against flipping over the top/bottom. Detect if flipped over the top by old up and new up facing away from each other. 
		if (glm::dot(newUp, Up) > 0)
		{
			// We didn't flip. All good!
			Position = newPosition;
			Forward = newForward;
			Right = newRight;
			Up = newUp;
		}
		else
		{
			// We flipped, so lets compute again with only the yaw
			// TODO It can be jittery at extremes. Also, needs optimisation
			
			newCamDir = glm::vec3{ glm::vec4{ camDir,0 } * yawMat };

			Position = Target + newCamDir * glm::length(camDisplacement);
			Forward = glm::normalize(Target - Position);
			Right = glm::normalize(glm::cross(Up, Forward));
			Up = glm::normalize(glm::cross(Forward, Right));
		}
	}

private:
	glm::vec3 _worldUp{ 0, 1, 0 };
};
