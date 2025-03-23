#include <core/framework.h>
#include <render/parameterset.h>
#include <render/rasterview.h>
#include <render/texture.h>
#include <render/pipelineobject.h>

#include "shaders/fluid.hlsli"
#include "fluidrendermodule.h"
#include "fluiddebugrendermodule.h"

using namespace cauldron;

FluidDebugRenderModule::FluidDebugRenderModule() : RenderModule(L"FluidDebugRenderModule")
{
}

void FluidDebugRenderModule::Init(const json& initData)
{
    m_pColorTarget     = GetFramework()->GetRenderTexture(L"GBufferAlbedoRT");
    m_pColorTarget     = GetFramework()->GetColorTargetForCallback(GetName());
    m_pColorRasterView = GetRasterViewAllocator()->RequestRasterView(m_pColorTarget, ViewDimension::Texture2D);

    RootSignatureDesc rsDesc;
    rsDesc.AddBufferUAVSet(0, ShaderBindStage::Pixel, 1);
    rsDesc.AddConstantBufferView(0, ShaderBindStage::Pixel, 1);

    m_sdfRS = std::unique_ptr<RootSignature>(RootSignature::CreateRootSignature(L"FluidDebugSDF", rsDesc));

    BlendDesc blendDesc;
    blendDesc.BlendEnabled     = true;
    blendDesc.SourceBlendColor = Blend::SrcAlpha;
    blendDesc.DestBlendColor   = Blend::InvSrcAlpha;
    blendDesc.ColorOp          = BlendOp::Add;
    blendDesc.SourceBlendAlpha = Blend::One;
    blendDesc.DestBlendAlpha   = Blend::Zero;
    blendDesc.AlphaOp          = BlendOp::Add;

    PipelineDesc psoDesc;
    psoDesc.SetRootSignature(m_sdfRS.get());
    psoDesc.AddShaderDesc(ShaderBuildDesc::Vertex(L"fullscreen.hlsl", L"FullscreenVS", ShaderModel::SM6_0, nullptr));
    psoDesc.AddShaderDesc(ShaderBuildDesc::Pixel(L"sdfdebug.hlsl", L"ps_main", ShaderModel::SM6_0, nullptr));
    psoDesc.AddPrimitiveTopology(PrimitiveTopologyType::Triangle);
    psoDesc.AddBlendStates({blendDesc}, false, false);
    psoDesc.AddRasterFormats(m_pColorTarget->GetFormat());

    m_sdfPSO = std::unique_ptr<PipelineObject>(PipelineObject::CreatePipelineObject(L"FluidDebugSDF", psoDesc));
    m_sdfPS = std::unique_ptr<ParameterSet>(ParameterSet::CreateParameterSet(m_sdfRS.get()));
    m_sdfPS->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(SceneInformation), 0);

    SetModuleEnabled(false);
    SetModuleReady(true);
}

void FluidDebugRenderModule::Execute(double dt, CommandList* pCmdList)
{
    if (!m_fluidRenderModule || !m_fluidRenderModule->ModuleEnabled())
        return;

    Barrier b = Barrier::Transition(m_pColorTarget->GetResource(), ResourceState::ShaderResource, ResourceState::RenderTargetResource);
    ResourceBarrier(pCmdList, 1, &b);

    m_sdfPS->UpdateRootConstantBuffer(m_fluidRenderModule->GetFluidInfoBuffer(), 0);

    BeginRaster(pCmdList, 1, &m_pColorRasterView);

    m_sdfPS->SetBufferUAV(m_fluidRenderModule->GetShaderParticleBuffer(), 0);
    m_sdfPS->Bind(pCmdList, m_sdfPSO.get());

    const ResolutionInfo& resInfo = GetFramework()->GetResolutionInfo();
    Viewport              vp      = {0.f, 0.f, resInfo.fDisplayWidth(), resInfo.fDisplayHeight(), 0.f, 1.f};
    SetViewport(pCmdList, &vp);
    Rect scissorRect = {0, 0, resInfo.RenderWidth, resInfo.RenderHeight};
    SetScissorRects(pCmdList, 1, &scissorRect);
    SetPrimitiveTopology(pCmdList, PrimitiveTopology::TriangleList);

    SetPipelineState(pCmdList, m_sdfPSO.get());
    DrawInstanced(pCmdList, 3);

    EndRaster(pCmdList);

    b = Barrier::Transition(m_pColorTarget->GetResource(), ResourceState::RenderTargetResource, ResourceState::ShaderResource);
    ResourceBarrier(pCmdList, 1, &b);
}
