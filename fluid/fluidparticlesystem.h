#pragma once

#include <vector>
#include "shaders/fluid.hlsli"

class FluidParticleSystem
{
public:
    struct CPUParticle
    {
        Point3 position;
        Vec3   velocity;
        float  radius;
        float  age;
    };

    void Update(float dt, Point3 cameraPos);

    std::vector<GPUParticle> CreateGPUParticles() const;

    float GetBiggestRadius(const std::vector<GPUParticle>& particles) const;

    int32_t m_maxParticles = 128;
    float   m_particleSize = .1f;
    float   m_updateSpeed  = 1;

    float m_positionX = 0;
    float m_positionY = 1;
    float m_positionZ = 0;

private:
    std::vector<CPUParticle> m_particles;
    Point3 m_cameraPos{};

    CPUParticle CreateParticle() const;
};
