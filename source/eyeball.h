// Object to store information about the Eyeball rotations
#pragma once

#include "DeviceResources.h"

class Eyeball
{

public:
	Eyeball();
	~Eyeball();
	int GetEyeballCounter();
	void IterateEyeballCounter();

	SimpleMath::Quaternion GenerateNewTarget();
	SimpleMath::Quaternion GetTarget();
	SimpleMath::Quaternion GetOrientation();

private:
	int												m_eyeballCounter;
	bool											m_returned;
	SimpleMath::Quaternion							m_eyeballTarget;
	SimpleMath::Quaternion							m_orientation;

};