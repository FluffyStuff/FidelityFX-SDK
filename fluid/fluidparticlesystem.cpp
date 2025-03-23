#include <algorithm>
#include "fluidparticlesystem.h"

namespace
{
float RandFloat(float min, float max)
{
    return min + (max - min) * rand() / (float)RAND_MAX;
}
}

FluidParticleSystem::CPUParticle FluidParticleSystem::CreateParticle() const
{
    Vec3 dir(1.5f, 0, 0);

    return CPUParticle
    ({
        Point3(m_positionX, m_positionY, m_positionZ),
        {
            dir.getX() + RandFloat(-.5f, .5f),
            dir.getY() + RandFloat(-.5f, .5f),
            dir.getZ() + RandFloat(-.5f, .5f),
        },
        .001f,
        RandFloat(0, 10)
    });
}

void FluidParticleSystem::Update(float dt, Point3 cameraPos)
{
    m_cameraPos = cameraPos;

    dt *= m_updateSpeed;

    if (m_particles.size() < m_maxParticles)
    {
        m_particles.push_back(CreateParticle());
    }
    else if (m_particles.size() > m_maxParticles)
    {
        m_particles.resize(m_maxParticles);
    }

    constexpr auto Gravity             = -1.f;
    constexpr auto VelocityAttenuation =  1.f;

    for (auto& particle : m_particles)
    {
        particle.age += dt;

        Vec3 velocity = particle.velocity;
        velocity.setY(velocity.getY() + Gravity * dt);

        particle.velocity = velocity * std::max(1 - VelocityAttenuation * dt, 0.f);
        particle.position += velocity * dt;

        // Intentionally ignore the radius, since the particles look better halfway into the floor
        if (particle.position.getY() < 0)
        {
            particle.position.setY(0);
            particle.velocity.setY(0);
        }

        if (particle.age < 10)
        {
            particle.radius += .2f * dt;
            particle.radius = std::min(particle.radius, m_particleSize);
        }
        else
        {
            particle.radius -= .2f * dt;
        }

        if (particle.radius <= 0)
        {
            particle = CreateParticle();
        }
    }
}

std::vector<GPUParticle> FluidParticleSystem::CreateGPUParticles() const
{
    std::vector<GPUParticle> particles;

    for (const auto& particle : m_particles)
    {
        if (particle.radius <= 0)
            continue;

        particles.push_back
        ({{
            particle.position.getX(),
            particle.position.getY(),
            particle.position.getZ(),
            particle.radius,
        }});
    }

    // Sort by distance from camera for optimized SDF tracing
    // We could do this on the GPU, but lets just do it CPU side for now
    auto cam = m_cameraPos;
    std::sort(std::begin(particles), std::end(particles), [cam](const GPUParticle& lhs, const GPUParticle& rhs)
    {
        float dist1 = distSqr(cam, Point3(lhs.posSize.getXYZ()));
        float dist2 = distSqr(cam, Point3(rhs.posSize.getXYZ()));
        return dist1 < dist2;
    });

    if (particles.size() > MAX_PARTICLES)
        particles.resize(MAX_PARTICLES);

    return particles;
}

float FluidParticleSystem::GetBiggestRadius(const std::vector<GPUParticle>& particles) const
{
    if (particles.empty())
        return 0;

    auto m = std::max_element(std::begin(particles), std::end(particles), [](auto const& lhs, auto const& rhs)
    {
        float w1 = lhs.posSize.getW();
        float w2 = rhs.posSize.getW();
        return w1 < w2;
    });

    return m->posSize.getW();
}

