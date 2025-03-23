#pragma once

#include "core/framework.h"

class FluidRenderModule;
class FluidDebugRenderModule;

class FluidFramework : public cauldron::Framework
{
public:
    FluidFramework(const cauldron::FrameworkInitParams* pInitParams) : cauldron::Framework(pInitParams) {}

    virtual void ParseSampleConfig() override;
    virtual void RegisterSampleModules() override;

    virtual void DoSampleInit() override;
    virtual void DoSampleUpdates(double dt) override;

private:
    FluidRenderModule*      m_fluidRenderModule      = nullptr;
    FluidDebugRenderModule* m_fluidDebugRenderModule = nullptr;

    bool m_renderFluid     = true;
    bool m_renderTileDebug = false;
};

using FrameworkType = FluidFramework;
