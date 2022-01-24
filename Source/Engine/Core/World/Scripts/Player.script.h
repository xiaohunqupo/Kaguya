#pragma once
#include "../Actor.h"
#include "../Components.h"
#include "Core/Application.h"

// Serves as a default player script that allows me to control the camera viewed by the main renderer
class PlayerScript : public ScriptableActor
{
public:
	void OnCreate() override
	{
		LastForward = 0.0f;
		LastStrafe	= 0.0f;
		LastAscent	= 0.0f;
	}

	void OnDestroy() override
	{
		
	}

	void OnUpdate(float DeltaTime) override
	{
		auto& Camera = GetComponent<CameraComponent>();

		bool Fwd = false, Bwd = false, Right = false, Left = false, Up = false, Down = false;
		if (Application::InputManager.RawInputEnabled)
		{
			Fwd	  = Application::InputManager.IsPressed('W');
			Bwd	  = Application::InputManager.IsPressed('S');
			Right = Application::InputManager.IsPressed('D');
			Left  = Application::InputManager.IsPressed('A');
			Up	  = Application::InputManager.IsPressed('E');
			Down  = Application::InputManager.IsPressed('Q');
		}

		if (Application::InputManager.RawInputEnabled)
		{
			float z = Camera.MovementSpeed * ((Fwd * DeltaTime) + (Bwd * -DeltaTime));
			float x = Camera.StrafeSpeed * ((Right * DeltaTime) + (Left * -DeltaTime));
			float y = Camera.StrafeSpeed * ((Up * DeltaTime) + (Down * -DeltaTime));

			if (Camera.Momentum)
			{
				ApplyMomentum(LastForward, z, DeltaTime);
				ApplyMomentum(LastStrafe, x, DeltaTime);
				ApplyMomentum(LastAscent, y, DeltaTime);
			}

			Camera.Translate(x, y, z);
		}

		Camera.Update();
	}

	// https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/CameraController.cpp
	void ApplyMomentum(float& OldValue, float& NewValue, float DeltaTime)
	{
		float BlendedValue;
		if (abs(NewValue) > abs(OldValue))
		{
			BlendedValue = lerp(NewValue, OldValue, pow(0.6f, DeltaTime * 60.0f));
		}
		else
		{
			BlendedValue = lerp(NewValue, OldValue, pow(0.8f, DeltaTime * 60.0f));
		}
		OldValue = BlendedValue;
		NewValue = BlendedValue;
	}

	float LastForward;
	float LastStrafe;
	float LastAscent;
};
