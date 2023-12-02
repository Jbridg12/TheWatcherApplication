#include "pch.h"
#include "eyeball.h"

Eyeball::Eyeball()
{
    m_eyeballCounter = 0;
    m_returned = false;
    m_eyeballTarget = SimpleMath::Quaternion::Identity;
}

Eyeball::~Eyeball()
{
}

int Eyeball::GetEyeballCounter()
{
    return m_eyeballCounter;
}

void Eyeball::IterateEyeballCounter()
{
    m_eyeballCounter++;
}

SimpleMath::Quaternion Eyeball::GenerateNewTarget()
{
    if (!m_returned)
    {
        SimpleMath::Quaternion temp;
        temp = m_eyeballTarget;
        m_eyeballTarget = m_orientation;
        m_orientation = temp;
        return m_eyeballTarget;
    }

    // Generate random small quaternion rotation to interpolate towards 

    SimpleMath::Quaternion target;

    float randY = ((rand() % 100) / 100.f) - 0.5f;
    float randX = ((rand() % 100) / 100.f) - 0.5f;
    float randZ = ((rand() % 100) / 100.f) - 0.5f;

    target = SimpleMath::Quaternion::CreateFromYawPitchRoll(randY, randX, randZ);
    target.Normalize();

    m_orientation = m_eyeballTarget;
    m_eyeballTarget = target;

    return m_eyeballTarget;
}

SimpleMath::Quaternion Eyeball::GetTarget()
{
    // Use counter to decide when to generate a new quaternion
    if (m_eyeballCounter % 20 == 0) {
        m_eyeballCounter = 0;
        m_returned = !m_returned;
        return GenerateNewTarget();
    }
    
    return m_eyeballTarget;
    
}

SimpleMath::Quaternion Eyeball::GetOrientation()
{
    // Save quaternion representation of the eyeball orientation
    return m_orientation;
}
