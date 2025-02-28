/*****************************************************************************
 * Copyright (c) 2018-2022 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "Camera.h"
#include "3D/LandIsland.h"
#include "ECS/Registry.h"
#include "Game.h"

#include <glm/gtx/euler_angles.hpp>

using namespace openblack;

glm::mat4 Camera::getRotationMatrix() const
{
	return glm::eulerAngleZXY(_rotation.z, _rotation.x, _rotation.y);
}

glm::mat4 Camera::GetViewMatrix() const
{
	return getRotationMatrix() * glm::translate(glm::mat4(1.0f), -_position);
}

glm::mat4 Camera::GetViewProjectionMatrix() const
{
	return GetProjectionMatrix() * GetViewMatrix();
}

void Camera::SetProjectionMatrixPerspective(float xFov, float aspect, float nearClip, float farClip)
{
	float yFov = (glm::atan(glm::tan(glm::radians(xFov) / 2.0f)) / aspect) * 2.0f;
	_projectionMatrix = glm::perspective(yFov, aspect, nearClip, farClip);
}

glm::vec3 Camera::GetForward() const
{
	// Forward is +1 in openblack but is -1 in OpenGL
	glm::mat3 mRotation = glm::transpose(getRotationMatrix());
	return mRotation * glm::vec3(0, 0, 1);
}

glm::vec3 Camera::GetRight() const
{
	glm::mat3 mRotation = glm::transpose(getRotationMatrix());
	return mRotation * glm::vec3(1, 0, 0);
}

glm::vec3 Camera::GetUp() const
{
	glm::mat3 mRotation = glm::transpose(getRotationMatrix());
	return mRotation * glm::vec3(0, 1, 0);
}

std::unique_ptr<Camera> Camera::Reflect(const glm::vec4& relectionPlane) const
{
	auto reflectionCamera = std::make_unique<ReflectionCamera>(_position, glm::degrees(_rotation), relectionPlane);
	reflectionCamera->SetProjectionMatrix(_projectionMatrix);

	return reflectionCamera;
}

void Camera::DeprojectScreenToWorld(const glm::ivec2 screenPosition, const glm::ivec2 screenSize, glm::vec3& out_worldOrigin,
                                    glm::vec3& out_worldDirection)
{
	const float normalizedX = (float)screenPosition.x / (float)screenSize.x;
	const float normalizedY = (float)screenPosition.y / (float)screenSize.y;

	const float screenSpaceX = (normalizedX - 0.5f) * 2.0f;
	const float screenSpaceY = ((1.0f - normalizedY) - 0.5f) * 2.0f;

	// The start of the ray trace is defined to be at mousex,mousey,1 in
	// projection space (z=0 is near, z=1 is far - this gives us better
	// precision) To get the direction of the ray trace we need to use any z
	// between the near and the far plane, so let's use (mousex, mousey, 0.5)
	const glm::vec4 rayStartProjectionSpace = glm::vec4(screenSpaceX, screenSpaceY, 0.0f, 1.0f);
	const glm::vec4 rayEndProjectionSpace = glm::vec4(screenSpaceX, screenSpaceY, 0.5f, 1.0f);

	// Calculate our inverse view projection matrix
	glm::mat4 inverseViewProj = glm::inverse(GetViewProjectionMatrix());

	// Get our homogeneous coordinates for our start and end ray positions
	const glm::vec4 hgRayStartWorldSpace = inverseViewProj * rayStartProjectionSpace;
	const glm::vec4 hgRayEndWorldSpace = inverseViewProj * rayEndProjectionSpace;

	glm::vec3 rayStartWorldSpace(hgRayStartWorldSpace.x, hgRayStartWorldSpace.y, hgRayStartWorldSpace.z);
	glm::vec3 rayEndWorldSpace(hgRayEndWorldSpace.x, hgRayEndWorldSpace.y, hgRayEndWorldSpace.z);

	// divide vectors by W to undo any projection and get the 3-space coord
	if (hgRayStartWorldSpace.w != 0.0f)
		rayStartWorldSpace /= hgRayStartWorldSpace.w;

	if (hgRayEndWorldSpace.w != 0.0f)
		rayEndWorldSpace /= hgRayEndWorldSpace.w;

	const glm::vec3 rayDirWorldSpace = glm::normalize(rayEndWorldSpace - rayStartWorldSpace);

	// finally, store the results in the outputs
	out_worldOrigin = rayStartWorldSpace;
	out_worldDirection = rayDirWorldSpace;
}

bool Camera::ProjectWorldToScreen(const glm::vec3 worldPosition, const glm::vec4 viewport, glm::vec3& out_screenPosition) const
{
	out_screenPosition = glm::project(worldPosition, GetViewMatrix(), GetProjectionMatrix(), viewport);
	if (out_screenPosition.x < viewport.x || out_screenPosition.y < viewport.y || out_screenPosition.x > viewport.z ||
	    out_screenPosition.y > viewport.w)
	{
		return false;
	}
	if (out_screenPosition.z > 1.0f)
	{
		// Behind Camera
		return false;
	}

	if (out_screenPosition.z < 0.0f)
	{
		// Clipped
		return false;
	}

	return true;
}

void Camera::ProcessSDLEvent(const SDL_Event& e)
{
	if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP)
		handleKeyboardInput(e);
	else if (e.type == SDL_MOUSEMOTION || e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP)
		handleMouseInput(e);
}

void Camera::handleKeyboardInput(const SDL_Event& e)
{
	// ignore all repeated keys
	if (e.key.repeat > 0)
		return;

	if (e.key.keysym.scancode == SDL_SCANCODE_W)
	{
		_dv.z += (e.type == SDL_KEYDOWN) ? 1.0f : -1.0f;
	}
	else if (e.key.keysym.scancode == SDL_SCANCODE_S)
	{
		_dv.z += (e.type == SDL_KEYDOWN) ? -1.0f : 1.0f;
	}
	else if (e.key.keysym.scancode == SDL_SCANCODE_A)
	{
		_dv.x += (e.type == SDL_KEYDOWN) ? -1.0f : 1.0f;
	}
	else if (e.key.keysym.scancode == SDL_SCANCODE_D)
	{
		_dv.x += (e.type == SDL_KEYDOWN) ? 1.0f : -1.0f;
	}
	else if (e.key.keysym.scancode == SDL_SCANCODE_LCTRL)
	{
		_dv.y += (e.type == SDL_KEYDOWN) ? -1.0f : 1.0f;
	}
	else if (e.key.keysym.scancode == SDL_SCANCODE_SPACE)
	{
		_dv.y += (e.type == SDL_KEYDOWN) ? 1.0f : -1.0f;
	}
}

void Camera::handleMouseInput(const SDL_Event& e)
{
	// Holding down the middle mouse button enables free look.
	if (e.type == SDL_MOUSEMOTION && e.motion.state & SDL_BUTTON(SDL_BUTTON_MIDDLE))
	{
		glm::vec3 rot = GetRotation();

		rot.y -= e.motion.xrel * _freeLookSensitivity * 0.1f;
		rot.x -= e.motion.yrel * _freeLookSensitivity * 0.1f;

		SetRotation(rot);
	}
	else if (e.type == SDL_MOUSEMOTION && e.motion.state & SDL_BUTTON(SDL_BUTTON_LEFT))
	{
		const auto& land = Game::instance()->GetLandIsland();
		auto momentum = _position.y / 300;
		auto forward = GetForward() * static_cast<float>(e.motion.yrel * momentum);
		auto right = GetRight() * -static_cast<float>(e.motion.xrel * momentum);
		auto futurePosition = _position + forward + right;
		auto height = land.GetHeightAt(glm::vec2(futurePosition.x + 5, futurePosition.z + 5)) + 13.0f;
		futurePosition.y = std::max(height, _position.y);
		_position = futurePosition;
	}
}

void Camera::Update(std::chrono::microseconds dt)
{
	auto accelFactor = 0.001f;
	auto airResistance = .9f;
	glm::mat3 rotation = glm::transpose(GetViewMatrix());
	_velocity += (((_dv * _maxMovementSpeed) - _velocity) * accelFactor);
	_position += rotation * _velocity * float(dt.count());
	_velocity *= airResistance;
}

glm::mat4 ReflectionCamera::GetViewMatrix() const
{
	glm::mat4 mRotation = getRotationMatrix();
	glm::mat4 mView = mRotation * glm::translate(glm::mat4(1.0f), -_position);

	// M''camera = Mreflection * Mcamera * Mflip
	glm::mat4x4 reflectionMatrix;
	reflectMatrix(reflectionMatrix, _reflectionPlane);

	return mView * reflectionMatrix;
}

/*
              | 1-2Nx2   -2NxNy  -2NxNz  -2NxD |
Mreflection = |  -2NxNy 1-2Ny2   -2NyNz  -2NyD |
              |  -2NxNz  -2NyNz 1-2Nz2   -2NzD |
              |    0       0       0       1   |
*/
void ReflectionCamera::reflectMatrix(glm::mat4x4& m, const glm::vec4& plane) const
{
	m[0][0] = (1.0f - 2.0f * plane[0] * plane[0]);
	m[1][0] = (-2.0f * plane[0] * plane[1]);
	m[2][0] = (-2.0f * plane[0] * plane[2]);
	m[3][0] = (-2.0f * plane[3] * plane[0]);

	m[0][1] = (-2.0f * plane[1] * plane[0]);
	m[1][1] = (1.0f - 2.0f * plane[1] * plane[1]);
	m[2][1] = (-2.0f * plane[1] * plane[2]);
	m[3][1] = (-2.0f * plane[3] * plane[1]);

	m[0][2] = (-2.0f * plane[2] * plane[0]);
	m[1][2] = (-2.0f * plane[2] * plane[1]);
	m[2][2] = (1.0f - 2.0f * plane[2] * plane[2]);
	m[3][2] = (-2.0f * plane[3] * plane[2]);

	m[0][3] = 0.0f;
	m[1][3] = 0.0f;
	m[2][3] = 0.0f;
	m[3][3] = 1.0f;
}
