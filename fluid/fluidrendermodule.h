#pragma once

#include <render/rendermodule.h>

namespace cauldron
{
    class Buffer;
    class Texture;
    class RasterView;
    class ParameterSet;
    class RootSignature;
    class PipelineObject;
    struct BufferAddressInfo;

    class UISection;
    class UIElement;
}

class FluidParticleSystem;
struct GPUParticle;

class FluidRenderModule : public cauldron::RenderModule
{
public:
    FluidRenderModule();
    virtual ~FluidRenderModule();

    void Init(const json& initData) override;
    void InitUI(cauldron::UISection* uiSection);
    void EnableModule(bool enabled) override;
    void Execute(double dt, cauldron::CommandList* pCmdList) override;

    void UpdateUI(double dt) override;

private:
    void InitResources();

    void UpdateFluidInfoBuffer();
    void UpdateParticleBuffer(cauldron::CommandList* pCmdList);
    void DispatchTiling(cauldron::CommandList* pCmdList);
    void RenderSDF(cauldron::CommandList* pCmdList);

    std::unique_ptr<FluidParticleSystem> m_particles;
    std::vector<GPUParticle> m_gpuParticles;

    uint32_t m_frame = 0;
    int32_t m_tilesX = 96;
    int32_t m_tilesY = 54;
    float m_sdfBlend = .25f;
    float m_triplanarBlend = 0.3f;
    float m_uvScaling = 2;

    const cauldron::Texture* m_pColorTarget         = nullptr;
    const cauldron::Texture* m_pAlbedoTarget        = nullptr;
    const cauldron::Texture* m_pDepthTarget         = nullptr;
    const cauldron::Texture* m_pNormalTarget        = nullptr;

    const cauldron::RasterView* m_pColorRasterView  = nullptr;
    const cauldron::RasterView* m_pAlbedoRasterView = nullptr;
    const cauldron::RasterView* m_pNormalRasterView = nullptr;
    const cauldron::RasterView* m_pDepthRasterView  = nullptr;

    std::unique_ptr<cauldron::ParameterSet>   m_sdfPS;
    std::unique_ptr<cauldron::RootSignature>  m_sdfRS;
    std::unique_ptr<cauldron::PipelineObject> m_sdfPSO;
    std::unique_ptr<cauldron::ParameterSet>   m_tilingPS;
    std::unique_ptr<cauldron::RootSignature>  m_tilingRS;
    std::unique_ptr<cauldron::PipelineObject> m_tilingPSO;

    std::unique_ptr<cauldron::BufferAddressInfo>   m_fluidInfoBuffer;
    std::unique_ptr<cauldron::Buffer>              m_shaderParticleBuffer;
    std::vector<std::unique_ptr<cauldron::Buffer>> m_particleBuffers;

    std::vector<cauldron::UIElement*> m_UIElements;

    cauldron::Buffer* GetCurrentParticleBuffer() const { return m_particleBuffers[m_frame % m_particleBuffers.size()].get(); }

public:
    const cauldron::BufferAddressInfo* GetFluidInfoBuffer() const { return m_fluidInfoBuffer.get(); }
    const cauldron::Buffer* GetShaderParticleBuffer() const { return m_shaderParticleBuffer.get(); }
};
