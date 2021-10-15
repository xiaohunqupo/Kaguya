#include "Player.script.h"

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

void PlayerScript::OnCreate()
{
	LastForward = 0.0f;
	LastStrafe	= 0.0f;
	LastAscent	= 0.0f;
}

void PlayerScript::OnDestroy()
{
}

void PlayerScript::OnUpdate(float DeltaTime)
{
	using namespace DirectX;
	using namespace physx;

	InputHandler& InputHandler = Application::GetInputHandler();
	Mouse&		  Mouse		   = Application::GetMouse();
	Keyboard&	  Keyboard	   = Application::GetKeyboard();

	auto& TransformComp = GetComponent<Transform>();
	auto& CameraComp	= GetComponent<Camera>();
	// auto& controller = GetComponent<CharacterController>();

	bool Fwd = false, Bwd = false, Right = false, Left = false, Up = false, Down = false;
	if (InputHandler.RawInputEnabled)
	{
		Fwd	  = Keyboard.IsPressed('W');
		Bwd	  = Keyboard.IsPressed('S');
		Right = Keyboard.IsPressed('D');
		Left  = Keyboard.IsPressed('A');
		Up	  = Keyboard.IsPressed('E');
		Down  = Keyboard.IsPressed('Q');

		// if (Keyboard.IsPressed(' '))
		//{
		//	if (canJump(controller.Controller))
		//	{
		//		Jump.startJump(30.0f);
		//	}
		//}
	}

	auto	 v = CameraComp.pTransform->Forward();
	XMFLOAT3 viewDir;
	XMStoreFloat3(&viewDir, v);

	PxVec3 disp;

	const PxF32 heightDelta = Jump.getHeight(DeltaTime);
	float		dy			= Gravity * DeltaTime;
	if (heightDelta != 0.0f)
	{
		dy = heightDelta;
	}

	PxVec3 targetKeyDisplacement(0);

	PxVec3 forward = ToPxVec3(viewDir);
	forward.y	   = 0;
	forward.normalize();
	PxVec3 up	 = PxVec3(0, 1, 0);
	PxVec3 right = up.cross(forward);

	{
		if (InputHandler.RawInputEnabled)
		{
			float z = CameraComp.MovementSpeed * ((Fwd * DeltaTime) + (Bwd * -DeltaTime));
			float x = CameraComp.StrafeSpeed * ((Right * DeltaTime) + (Left * -DeltaTime));
			float y = CameraComp.StrafeSpeed * ((Up * DeltaTime) + (Down * -DeltaTime));

			if (CameraComp.Momentum)
			{
				ApplyMomentum(LastForward, z, DeltaTime);
				ApplyMomentum(LastStrafe, x, DeltaTime);
				ApplyMomentum(LastAscent, y, DeltaTime);
			}

			CameraComp.Translate(x, y, z);

			targetKeyDisplacement += z * forward;
			targetKeyDisplacement += x * right;

			/*if (Keyboard.IsPressed('W'))	targetKeyDisplacement += m_MoveSpeed * forward;
			if (Keyboard.IsPressed('S'))	targetKeyDisplacement -= m_MoveSpeed * forward;

			if (Keyboard.IsPressed('A'))	targetKeyDisplacement += m_StrafeSpeed * right;
			if (Keyboard.IsPressed('D'))	targetKeyDisplacement -= m_StrafeSpeed * right;*/

			while (const auto e = Mouse.ReadRawInput())
			{
				CameraComp.Rotate(
					e->y * DeltaTime * CameraComp.MouseSensitivityY,
					e->x * DeltaTime * CameraComp.MouseSensitivityX,
					0.0f);
			}
		}

		// targetKeyDisplacement *= mKeyShiftDown ? mRunningSpeed : mWalkingSpeed;
		// targetKeyDisplacement *= dt;
	}

	disp   = targetKeyDisplacement;
	disp.y = dy;

	// PxControllerFilters				  filters;
	// physx::PxControllerCollisionFlags flags = controller.Controller->move(disp, 0.0f, dt, filters);
	// if (flags & PxControllerCollisionFlag::eCOLLISION_DOWN)
	//{
	//	Jump.stopJump();
	//}

	// const auto& p			  = controller.Controller->getFootPosition();
	// camera.Transform.Position = XMFLOAT3(float(p.x), float(p.y + 1.75f), float(p.z));

	// if (InputHandler.RawInputEnabled)
	//{
	//	float forward = m_MoveSpeed * (
	//		(Keyboard.IsPressed('W') * dt) +
	//		(Keyboard.IsPressed('S') * -dt));
	//	float strafe = m_StrafeSpeed * (
	//		(Keyboard.IsPressed('D') * dt) +
	//		(Keyboard.IsPressed('A') * -dt));
	//	float ascent = m_StrafeSpeed * (
	//		(Keyboard.IsPressed('E') * dt) +
	//		(Keyboard.IsPressed('Q') * -dt));
	//
	//	if (m_Momentum)
	//	{
	//		ApplyMomentum(m_LastForward, forward, dt);
	//		ApplyMomentum(m_LastStrafe, strafe, dt);
	//		ApplyMomentum(m_LastAscent, ascent, dt);
	//	}
	//
	//	camera.Transform.Translate(strafe, ascent, forward);
	//
	//}

	CameraComp.Update();
}
