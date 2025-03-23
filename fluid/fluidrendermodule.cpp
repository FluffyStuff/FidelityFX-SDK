#include <core/framework.h>
#include <core/scene.h>
#include <core/contentmanager.h>
#include <core/uimanager.h>
#include <render/swapchain.h>
#include <render/rasterview.h>
#include <render/pipelineobject.h>
#include <render/parameterset.h>

#include "shaders/fluid.hlsli"
#include "fluidparticlesystem.h"
#include "fluidrendermodule.h"

using namespace cauldron;

const std::array<std::tuple<std::experimental::filesystem::path, bool>, 4> TextureList
{
    std::tuple<std::experimental::filesystem::path, bool>
    {"../media/custom/lava/Lava004_2K-PNG_Color.png",        false},
    {"../media/custom/lava/Lava004_2K-PNG_Displacement.png",  true},
    {"../media/custom/lava/Lava004_2K-PNG_NormalDX.png",      true},
    {"../media/custom/lava/Lava004_2K-PNG_Roughness.png",     true},
};

FluidRenderModule::FluidRenderModule() : RenderModule(L"FluidRenderModule")
{
    m_fluidInfoBuffer = std::make_unique<BufferAddressInfo>();
    m_particles       = std::make_unique<FluidParticleSystem>();
}

FluidRenderModule::~FluidRenderModule()
{
    EnableModule(false);
}

void FluidRenderModule::Init(const json& initData)
{
    auto callback = [this](const std::vector<const Texture*>& textures, void* additionalParams) { InitResources(); };

    std::vector<TextureLoadInfo> textures;
    for (const auto& tex : TextureList) textures.push_back({std::get<0>(tex), std::get<1>(tex)});
    GetContentManager()->LoadTextures(textures, callback);
}

void FluidRenderModule::InitResources()
{
    // Resources
    m_pColorTarget      = GetFramework()->GetRenderTexture(L"GBufferAoRoughnessMetallicRT");
    m_pAlbedoTarget     = GetFramework()->GetRenderTexture(L"GBufferAlbedoRT");
    m_pNormalTarget     = GetFramework()->GetRenderTexture(L"GBufferNormalTarget");
    m_pDepthTarget      = GetFramework()->GetRenderTexture(L"DepthTarget");
    m_pColorRasterView  = GetRasterViewAllocator()->RequestRasterView(m_pColorTarget,  ViewDimension::Texture2D);
    m_pAlbedoRasterView = GetRasterViewAllocator()->RequestRasterView(m_pAlbedoTarget, ViewDimension::Texture2D);
    m_pNormalRasterView = GetRasterViewAllocator()->RequestRasterView(m_pNormalTarget, ViewDimension::Texture2D);
    m_pDepthRasterView  = GetRasterViewAllocator()->RequestRasterView(m_pDepthTarget,  ViewDimension::Texture2D);

    // Buffers
    {
        uint32_t   bufferSize    = sizeof(GPUParticle) * TILE_MAX_PARTICLES * TILES_MAX_X * TILES_MAX_Y;
        BufferDesc bufferSurface = BufferDesc::Data(L"SortedParticleBuffer", bufferSize, sizeof(GPUParticle), 0, ResourceFlags::AllowUnorderedAccess);
        m_shaderParticleBuffer = std::unique_ptr<Buffer>(Buffer::CreateBufferResource(&bufferSurface, ResourceState::UnorderedAccess));

        auto bufferCount = GetFramework()->GetSwapChain()->GetBackBufferCount();
        // Framework is flaky sometimes (let's just assume tripple buffering)
        if (!bufferCount)
            bufferCount = 3;
        for (int i = 0; i < bufferCount; i++)
        {
            std::wstring name          = L"ParticlesBuffer" + std::to_wstring(i);
            BufferDesc   bufferSurface = BufferDesc::Data(name.c_str(), sizeof(GPUParticle) * MAX_PARTICLES, sizeof(GPUParticle));
            m_particleBuffers.push_back(std::unique_ptr<Buffer>(Buffer::CreateBufferResource(&bufferSurface, ResourceState::ShaderResource)));
        }
    }

    // SDF shader
    {
        SamplerDesc linearSamplerDesc;
        linearSamplerDesc.AddressU = AddressMode::Wrap;
        linearSamplerDesc.AddressV = AddressMode::Wrap;
        linearSamplerDesc.AddressW = AddressMode::Wrap;

        RootSignatureDesc sdfRSDesc;
        sdfRSDesc.AddBufferUAVSet(0, ShaderBindStage::Pixel, 1); // ParticleBuffer
        sdfRSDesc.AddTextureSRVSet(1, ShaderBindStage::Pixel, 1); // DepthTexture
        sdfRSDesc.AddTextureSRVSet(2, ShaderBindStage::Pixel, (uint32_t)TextureList.size()); // Material textures
        sdfRSDesc.AddStaticSamplers(0, ShaderBindStage::Pixel, 1, &linearSamplerDesc);
        sdfRSDesc.AddConstantBufferView(0, ShaderBindStage::Pixel, 1);

        m_sdfRS = std::unique_ptr<RootSignature>(RootSignature::CreateRootSignature(L"FluidSDF", sdfRSDesc));

        PipelineDesc psoDesc;
        psoDesc.SetRootSignature(m_sdfRS.get());
        psoDesc.AddShaderDesc(ShaderBuildDesc::Vertex(L"fullscreen.hlsl", L"FullscreenVS", ShaderModel::SM6_0, nullptr));
        psoDesc.AddShaderDesc(ShaderBuildDesc::Pixel(L"sdf.hlsl", L"ps_main", ShaderModel::SM6_0, nullptr));
        psoDesc.AddPrimitiveTopology(PrimitiveTopologyType::Triangle);
        const std::vector<ResourceFormat> rtFormats = {m_pColorTarget->GetFormat(), m_pAlbedoTarget->GetFormat(), m_pNormalTarget->GetFormat()};
        psoDesc.AddRasterFormats(rtFormats, m_pDepthTarget->GetFormat());

        DepthDesc depthDesc;
        depthDesc.DepthEnable = true;
        depthDesc.DepthWriteEnable = true;
        psoDesc.AddDepthState(&depthDesc);

        m_sdfPSO = std::unique_ptr<PipelineObject>(PipelineObject::CreatePipelineObject(L"FluidSDF", psoDesc));

        m_sdfPS = std::unique_ptr<ParameterSet>(ParameterSet::CreateParameterSet(m_sdfRS.get()));
        m_sdfPS->SetTextureSRV(m_pDepthTarget, ViewDimension::Texture2D, 1);

        for (int i = 0; i < TextureList.size(); i++)
            m_sdfPS->SetTextureSRV(GetContentManager()->GetTexture(std::get<0>(TextureList[i])), ViewDimension::Texture2D, i + 2);

        m_sdfPS->SetBufferUAV(GetShaderParticleBuffer(), 0);
        m_sdfPS->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(SceneInformation), 0);
    }

    // Tiling shader
    {
        RootSignatureDesc tilingRSDesc;
        tilingRSDesc.AddConstantBufferView(0, ShaderBindStage::Compute, 1);
        tilingRSDesc.AddBufferSRVSet(0, ShaderBindStage::Compute, 1);
        tilingRSDesc.AddBufferUAVSet(0, ShaderBindStage::Compute, 1);

        m_tilingRS = std::unique_ptr<RootSignature>(RootSignature::CreateRootSignature(L"FluidTiling", tilingRSDesc));

        PipelineDesc tilingPsoDesc;
        tilingPsoDesc.SetRootSignature(m_tilingRS.get());
        tilingPsoDesc.AddShaderDesc(ShaderBuildDesc::Compute(L"tiling.hlsl", L"TilingCS", ShaderModel::SM6_0, nullptr));
        m_tilingPSO = std::unique_ptr<PipelineObject>(PipelineObject::CreatePipelineObject(L"FluidTiling", tilingPsoDesc));

        m_tilingPS = std::unique_ptr<ParameterSet>(ParameterSet::CreateParameterSet(m_tilingRS.get()));
        m_tilingPS->SetBufferUAV(GetShaderParticleBuffer(), 0);
        m_tilingPS->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(SceneInformation), 0);
    }

    SetModuleReady(true);
}

void FluidRenderModule::InitUI(UISection* uiSection)
{
    m_UIElements.emplace_back(uiSection->RegisterUIElement<UISlider<int32_t>>("Max particles", m_particles->m_maxParticles, 0, MAX_PARTICLES));
    m_UIElements.emplace_back(uiSection->RegisterUIElement<UISlider<int32_t>>("Tiles X", m_tilesX, 1, TILES_MAX_X));
    m_UIElements.emplace_back(uiSection->RegisterUIElement<UISlider<int32_t>>("Tiles Y", m_tilesY, 1, TILES_MAX_Y));
    m_UIElements.emplace_back(uiSection->RegisterUIElement<UISlider<float>>("SDF blend", m_sdfBlend, 0.f, 1.f));
    m_UIElements.emplace_back(uiSection->RegisterUIElement<UISlider<float>>("Particle size", m_particles->m_particleSize, 0.f, 1.f));
    m_UIElements.emplace_back(uiSection->RegisterUIElement<UISlider<float>>("Particle update speed", m_particles->m_updateSpeed, 0.f, 10.f));
    m_UIElements.emplace_back(uiSection->RegisterUIElement<UISlider<float>>("Particle spawner X", m_particles->m_positionX, -10.f, 10.f));
    m_UIElements.emplace_back(uiSection->RegisterUIElement<UISlider<float>>("Particle spawner Y", m_particles->m_positionY, -10.f, 10.f));
    m_UIElements.emplace_back(uiSection->RegisterUIElement<UISlider<float>>("Particle spawner Z", m_particles->m_positionZ, -10.f, 10.f));
    m_UIElements.emplace_back(uiSection->RegisterUIElement<UISlider<float>>("Triplanar blend", m_triplanarBlend, 0.f, 1.f));
    m_UIElements.emplace_back(uiSection->RegisterUIElement<UISlider<float>>("UV scaling", m_uvScaling, 0.f, 10.f));
}

void FluidRenderModule::UpdateUI(double dt)
{
    auto cameraPos = GetScene()->GetCurrentCamera()->GetCameraPos();
    m_particles->Update(static_cast<float>(dt), Point3(cameraPos));
    m_gpuParticles = m_particles->CreateGPUParticles();

    m_frame++;
}

void FluidRenderModule::UpdateFluidInfoBuffer()
{
    FluidInfo fluidInfo      = {};
    fluidInfo.SceneInfo      = GetScene()->GetSceneInfo();
    fluidInfo.ParticleCount  = static_cast<int>(m_gpuParticles.size());
    fluidInfo.TilesX         = m_tilesX;
    fluidInfo.TilesY         = m_tilesY;
    fluidInfo.BiggestRadius  = m_particles->GetBiggestRadius(m_gpuParticles);
    fluidInfo.SDFBlend       = m_sdfBlend;
    fluidInfo.TriplanarBlend = m_triplanarBlend;
    fluidInfo.UVScaling      = m_uvScaling;

    *m_fluidInfoBuffer = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(fluidInfo), &fluidInfo);
}

void FluidRenderModule::UpdateParticleBuffer(CommandList* pCmdList)
{
    if (m_gpuParticles.empty())
        return;

    auto particleBuffer = GetCurrentParticleBuffer();

    Barrier b = Barrier::Transition(particleBuffer->GetResource(), ResourceState::ShaderResource, ResourceState::CopyDest);
    ResourceBarrier(pCmdList, 1, &b);

    particleBuffer->CopyData(m_gpuParticles.data(), sizeof(GPUParticle) * m_gpuParticles.size());

    b = Barrier::Transition(particleBuffer->GetResource(), ResourceState::CopyDest, ResourceState::ShaderResource);
    ResourceBarrier(pCmdList, 1, &b);
}

void FluidRenderModule::DispatchTiling(CommandList* pCmdList)
{
    m_tilingPS->UpdateRootConstantBuffer(GetFluidInfoBuffer(), 0);
    m_tilingPS->SetBufferSRV(GetCurrentParticleBuffer(), 0);
    m_tilingPS->Bind(pCmdList, m_tilingPSO.get());
    SetPipelineState(pCmdList, m_tilingPSO.get());

    uint32_t numGroupX = DivideRoundingUp(m_tilesX, TILING_THREAD_X);
    uint32_t numGroupY = DivideRoundingUp(m_tilesY, TILING_THREAD_Y);
    Dispatch(pCmdList, numGroupX, numGroupY, 1);

    Barrier b = Barrier::UAV(GetShaderParticleBuffer()->GetResource());
    ResourceBarrier(pCmdList, 1, &b);
}

void FluidRenderModule::RenderSDF(CommandList* pCmdList)
{
    std::array<Barrier, 3> preBarriers
    {
        Barrier::Transition(m_pColorTarget->GetResource(),  ResourceState::ShaderResource, ResourceState::RenderTargetResource),
        Barrier::Transition(m_pAlbedoTarget->GetResource(), ResourceState::ShaderResource, ResourceState::RenderTargetResource),
        Barrier::Transition(m_pNormalTarget->GetResource(), ResourceState::ShaderResource, ResourceState::RenderTargetResource),
    };
    ResourceBarrier(pCmdList, (uint32_t)preBarriers.size(), preBarriers.data());

    std::array<const RasterView*, 3> rasters{m_pColorRasterView, m_pAlbedoRasterView, m_pNormalRasterView};
    BeginRaster(pCmdList, (uint32_t)rasters.size(), rasters.data(), m_pDepthRasterView);

    m_sdfPS->UpdateRootConstantBuffer(GetFluidInfoBuffer(), 0);
    m_sdfPS->Bind(pCmdList, m_sdfPSO.get());

    const ResolutionInfo& resInfo = GetFramework()->GetResolutionInfo();
    Viewport vp = {0.f, 0.f, resInfo.fDisplayWidth(), resInfo.fDisplayHeight(), 0.f, 1.f};
    SetViewport(pCmdList, &vp);
    Rect scissorRect = {0, 0, resInfo.RenderWidth, resInfo.RenderHeight};
    SetScissorRects(pCmdList, 1, &scissorRect);
    SetPrimitiveTopology(pCmdList, PrimitiveTopology::TriangleList);

    SetPipelineState(pCmdList, m_sdfPSO.get());
    DrawInstanced(pCmdList, 3);

    EndRaster(pCmdList);

    std::array<Barrier, 3> postBarriers
    {
        Barrier::Transition(m_pColorTarget->GetResource(),  ResourceState::RenderTargetResource, ResourceState::ShaderResource),
        Barrier::Transition(m_pAlbedoTarget->GetResource(), ResourceState::RenderTargetResource, ResourceState::ShaderResource),
        Barrier::Transition(m_pNormalTarget->GetResource(), ResourceState::RenderTargetResource, ResourceState::ShaderResource),
    };
    ResourceBarrier(pCmdList, (uint32_t)postBarriers.size(), postBarriers.data());
}

void FluidRenderModule::Execute(double dt, CommandList* pCmdList)
{
    UpdateFluidInfoBuffer();
    UpdateParticleBuffer(pCmdList);
    DispatchTiling(pCmdList);
    RenderSDF(pCmdList);
}

void FluidRenderModule::EnableModule(bool enabled)
{
    RenderModule::EnableModule(enabled);
    for (auto& i : m_UIElements)
    {
        i->Show(enabled);
    }
}
