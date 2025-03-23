#include <misc/fileio.h>
#include <core/uimanager.h>
#include <rendermoduleregistry.h>

#include "fluidrendermodule.h"
#include "fluiddebugrendermodule.h"
#include "fluidframework.h"

using namespace cauldron;

void FluidFramework::ParseSampleConfig()
{
    json sampleConfig;
    CauldronAssert(ASSERT_CRITICAL, ParseJsonFile(ConfigFileName, sampleConfig), L"Could not parse JSON file %ls", L"fluidconfig.json");
    ParseConfigData(sampleConfig["Fluid"]);
}

void FluidFramework::RegisterSampleModules()
{
    rendermodule::RegisterAvailableRenderModules();
    RenderModuleFactory::RegisterModule<FluidRenderModule>("FluidRenderModule");
    RenderModuleFactory::RegisterModule<FluidDebugRenderModule>("FluidDebugRenderModule");
}

void FluidFramework::DoSampleInit()
{
    m_fluidRenderModule      = static_cast<FluidRenderModule*>(GetFramework()->GetRenderModule("FluidRenderModule"));
    m_fluidDebugRenderModule = static_cast<FluidDebugRenderModule*>(GetFramework()->GetRenderModule("FluidDebugRenderModule"));

    UISection* uiSection = GetUIManager()->RegisterUIElements("Fluid", UISectionType::Sample);
    if (uiSection)
    {
        uiSection->RegisterUIElement<UICheckBox>("Render Fluid", m_renderFluid);
        uiSection->RegisterUIElement<UICheckBox>("Render Tile Debug", m_renderTileDebug);
        m_fluidRenderModule->InitUI(uiSection);
    }

    m_fluidDebugRenderModule->SetFluidRenderModule(m_fluidRenderModule);
}

void FluidFramework::DoSampleUpdates(double dt)
{
    m_fluidRenderModule->EnableModule(m_renderFluid);
    m_fluidDebugRenderModule->EnableModule(m_renderTileDebug);
}
