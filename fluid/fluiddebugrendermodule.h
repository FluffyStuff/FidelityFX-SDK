#pragma once

#include <render/rendermodule.h>

namespace cauldron
{
    class Texture;
    class RasterView;
    class ParameterSet;
    class RootSignature;
    class PipelineObject;
}

class FluidRenderModule;

class FluidDebugRenderModule : public cauldron::RenderModule
{
public:
    FluidDebugRenderModule();

    void Init(const json& initData) override;
    void Execute(double dt, cauldron::CommandList* pCmdList) override;

    void SetFluidRenderModule(const FluidRenderModule* module) { m_fluidRenderModule = module; }

private:
    const FluidRenderModule*    m_fluidRenderModule = nullptr;
    const cauldron::Texture*    m_pColorTarget      = nullptr;
    const cauldron::RasterView* m_pColorRasterView  = nullptr;

    std::unique_ptr<cauldron::ParameterSet>   m_sdfPS;
    std::unique_ptr<cauldron::RootSignature>  m_sdfRS;
    std::unique_ptr<cauldron::PipelineObject> m_sdfPSO;
};
