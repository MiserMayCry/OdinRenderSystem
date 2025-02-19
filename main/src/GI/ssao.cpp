#include <DirectXPackedVector.h>

#include "graphics/command_context.h"
#include "graphics/graphics_core.h"
#include "graphics/graphics_common.h"
#include "graphics/graphics_utility.h"
#include "graphics/sampler_manager.h"
#include "game/game_include.h"
#include "GI/gi_utility.h"
#include "GI/ssao.h"
#include "math_helper.h"
#include "utility.h"


namespace GI {

namespace AO {

using namespace DirectX;
using namespace DirectX::PackedVector;

Ssao MainSsao;


void Ssao::Initialize(UINT width, UINT height) {
  BuildOffsetVectors();

  BuildDrawNormalComponents();
  BuildAmbientComponents();

  Resize(width, height);

  is_enabled_ = GameSetting::GetBoolValue("UseSsao");
}

void Ssao::Resize(UINT width, UINT height) {
  if (width_ != width || height != height) {
    width_ = width;
    height_ = height;
    DebugUtility::Log(L"Ssao resize : width %0, height %1", to_wstring(width_), to_wstring(height_));

    //viewport_.TopLeftX = 0.0f;
    //viewport_.TopLeftY = 0.0f;
    //viewport_.Width = width ;
    //viewport_.Height = height;
    //viewport_.MinDepth = 0.0f;
    //viewport_.MaxDepth = 1.0f;

    //scissor_rect_ = { 0, 0, (int)width , (int)height  };

    normal_map_.Create(L"Ssao_Normal", width_, height_, 1, Ssao::kNormalMapFormat);
    depth_buffer_.Create(L"Ssao DepthBuffer", width_, height_, Graphics::Core.DepthStencilFormat);

    ambient_map_0_.Create(L"Ssao_Ambient_Map_0", width_, height_, 1, Ssao::kNormalMapFormat);

    ambient_map_1_.Create(L"Ssao_Ambient_Map_1", width_, height_, 1, Ssao::kNormalMapFormat);

    const uint32_t random_width = 64;
    const uint32_t random_height = 64;
    //const uint32_t map_size = random_width * random_height;
    //XMCOLOR random_buffer[map_size];

    //for (int i=0;i<random_height; ++i) {
    //  for (int j=0; j<random_width; ++j) {
    //    //XMFLOAT3 v(MathHelper::RandF(), MathHelper::RandF(), MathHelper::RandF());
    //    XMFLOAT3 v(1.0f, 1.0f, 1.0f);
    //    random_buffer[i * random_height + j] = XMCOLOR(v.x, v.y, v.z, 1.0f);
    //  }
    //}

    //random_map_.Create(L"Ssao_Random_Map", random_width, random_height, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
    //CommandContext::InitializeBuffer(random_map_, random_buffer, sizeof(XMCOLOR) * map_size);
  }
}

void Ssao::BuildDrawNormalComponents() {
  draw_normal_root_signature_.Reset(2, 0);
  draw_normal_root_signature_[0].InitAsConstantBufferView(0);
  draw_normal_root_signature_[1].InitAsConstantBufferView(1);
  draw_normal_root_signature_.Finalize();

  auto ssao_normal_vs = d3dUtil::CompileShader(L"shaders/ssao_normal.hlsl", nullptr, "VS", "vs_5_1");
  auto ssao_normal_ps = d3dUtil::CompileShader(L"shaders/ssao_normal.hlsl", nullptr, "PS", "ps_5_1");

  std::vector<D3D12_INPUT_ELEMENT_DESC> input_layout =
  {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    //  { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
  };

  draw_normal_pso_.SetInputLayout(input_layout.data(), (UINT)input_layout.size());
  draw_normal_pso_.SetVertexShader(reinterpret_cast<BYTE*>(ssao_normal_vs->GetBufferPointer()),
    (UINT)ssao_normal_vs->GetBufferSize());
  draw_normal_pso_.SetPixelShader(reinterpret_cast<BYTE*>(ssao_normal_ps->GetBufferPointer()),
    (UINT)ssao_normal_ps->GetBufferSize());

  draw_normal_pso_.SetRootSignature(draw_normal_root_signature_);
  draw_normal_pso_.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
  draw_normal_pso_.SetRasterizeState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
  draw_normal_pso_.SetDepthStencilState(CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT));
  draw_normal_pso_.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
  draw_normal_pso_.SetSampleMask(UINT_MAX);
  draw_normal_pso_.SetRenderTargetFormat(Ssao::kNormalMapFormat, Graphics::Core.DepthStencilFormat);
  draw_normal_pso_.Finalize();
}

void Ssao::BuildAmbientComponents() {

  SamplerDesc point_sampler;
  point_sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
  point_sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  point_sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  point_sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

  SamplerDesc linear_sampler;
  linear_sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
  linear_sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  linear_sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  linear_sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

  SamplerDesc depth_sampler;
  depth_sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
  depth_sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  depth_sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  depth_sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  depth_sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

  SamplerDesc linear_wrap_sampler;
  linear_wrap_sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
  linear_wrap_sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  linear_wrap_sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  linear_wrap_sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;


  ssao_root_signature_.Reset(3, 4);
  ssao_root_signature_[0].InitAsConstantBufferView(0);
  ssao_root_signature_[1].InitAsConstants(1, 1);
  ssao_root_signature_[2].InitAsDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
  ssao_root_signature_.InitSampler(0, point_sampler, D3D12_SHADER_VISIBILITY_PIXEL);
  ssao_root_signature_.InitSampler(1, linear_sampler, D3D12_SHADER_VISIBILITY_PIXEL);
  ssao_root_signature_.InitSampler(2, depth_sampler, D3D12_SHADER_VISIBILITY_PIXEL);
  ssao_root_signature_.InitSampler(3, linear_wrap_sampler, D3D12_SHADER_VISIBILITY_PIXEL);
  ssao_root_signature_.Finalize();

  auto ssao_standard_vs = d3dUtil::CompileShader(L"shaders/ssao_standard.hlsl", nullptr, "VS", "vs_5_1");
  auto ssao_standard_ps = d3dUtil::CompileShader(L"shaders/ssao_standard.hlsl", nullptr, "PS", "ps_5_1");

  std::vector<D3D12_INPUT_ELEMENT_DESC> input_layout =
  {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
  //  { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
  };

  ssao_pso_.SetInputLayout(input_layout.data(), (UINT)input_layout.size());
  ssao_pso_.SetVertexShader(reinterpret_cast<BYTE*>(ssao_standard_vs->GetBufferPointer()),
    (UINT)ssao_standard_vs->GetBufferSize());
  ssao_pso_.SetPixelShader(reinterpret_cast<BYTE*>(ssao_standard_ps->GetBufferPointer()),
    (UINT)ssao_standard_ps->GetBufferSize());

  ssao_pso_.SetRootSignature(ssao_root_signature_);
  ssao_pso_.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
  ssao_pso_.SetRasterizeState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
  ssao_pso_.SetDepthStencilState(Graphics::DepthStateDisabled);
  ssao_pso_.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
  ssao_pso_.SetSampleMask(UINT_MAX);
  ssao_pso_.SetRenderTargetFormat(Ssao::kNormalMapFormat, DXGI_FORMAT_UNKNOWN);
  ssao_pso_.Finalize();
}


void Ssao::ComputeAoInternal(GraphicsContext& context, XMFLOAT4X4& view, XMFLOAT4X4& proj) {
  XMMATRIX proj_matrix = XMLoadFloat4x4(&proj);
  XMMATRIX inv_proj_matrix = XMMatrixInverse(&XMMatrixDeterminant(proj_matrix), proj_matrix);

  XMStoreFloat4x4(&ssao_constants_.Proj, XMMatrixTranspose(proj_matrix));
  XMStoreFloat4x4(&ssao_constants_.InvProj, XMMatrixTranspose(inv_proj_matrix));
  XMStoreFloat4x4(&ssao_constants_.ProjTex, XMMatrixTranspose(proj_matrix * Ssao::kTextureTransform));
  ssao_constants_.InvRenderTargetSize = { 1.0f / width_, 1.0f / height_ };

  auto gauss_weight = GetGaussWeight(2.5f);

  ssao_constants_.BlurWeights[0] = XMFLOAT4(&gauss_weight[0]);
  ssao_constants_.BlurWeights[1] = XMFLOAT4(&gauss_weight[4]);
  ssao_constants_.BlurWeights[2] = XMFLOAT4(&gauss_weight[8]);

  memcpy(&ssao_constants_.OffsetVectors, offsets_, sizeof(&offsets_[0]));

  ssao_constants_.OcclusionRadius = 0.5f;
  ssao_constants_.OcclusionFadeStart = 0.2f;
  ssao_constants_.OcclusionFadeEnd = 1.0f;
  ssao_constants_.SurfaceEpsilon = 0.05f;

  //context.SetViewports(&viewport_);
  //context.SetScissorRects(&scissor_rect_);

  ColorBuffer* buffer = &ambient_map_0_;

  context.TransitionResource(*buffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
  context.ClearColor(*buffer);
  context.SetRenderTarget(&buffer->Rtv());

  context.SetRootSignature(ssao_root_signature_);
  context.SetPipelineState(ssao_pso_);

  context.SetDynamicConstantBufferView(0, sizeof(SsaoConstants), &ssao_constants_);

  context.SetRoot32BitConstant(1, 1, 0);

  D3D12_CPU_DESCRIPTOR_HANDLE handles[] = { normal_map_.Srv(), depth_buffer_.DepthSRV() };
  context.SetDynamicDescriptors(2, 0, _countof(handles), handles);

  context.SetVertexBuffers(0, 0, nullptr);
  context.SetNullIndexBuffer();
  context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  context.DrawInstanced(6, 1, 0, 0);
  context.TransitionResource(*buffer, D3D12_RESOURCE_STATE_GENERIC_READ, true);
  //  context.Flush(true);

}

void Ssao::ComputeAo() {
  if (!is_enabled_) 
    return;

  vector<RenderObject*> objects;
  auto begin = Graphics::MainQueue.Begin();
  for (auto it = begin; it != Graphics::MainQueue.End(); ++it) {

    auto group_begin = it->second->Begin();
    auto group_end = it->second->End();
    for (auto it_obj = group_begin; it_obj != group_end; ++it_obj) {
      objects.push_back((*it_obj));
    }
  }

  GraphicsContext& context = GraphicsContext::Begin(L"SSAO Context");

  XMFLOAT4X4& view = GameCore::MainCamera.View4x4f();
  XMFLOAT4X4& proj = GameCore::MainCamera.Proj4x4f();

  GI::Utility::DrawNormalAndDepthMap(context, normal_map_, depth_buffer_, objects);

  ComputeAoInternal(context, view, proj);

  int iterate_count = GameSetting::GetIntValue("SsaoBlurIterateCount");
  for (int i = 0; i < iterate_count; ++i) {
    //  BlurAmbientMap(context, false);
    //  BlurAmbientMap(context, true);
  }
  
  //context.Finish(true);
  ComputeContext& compute = reinterpret_cast<ComputeContext&>(context);
  Graphics::Utility::Blur(compute, ambient_map_0_, iterate_count);
  context.Finish(true);
}

void Ssao::BlurAmbientMap(GraphicsContext& context, bool is_horizontal) {
  int root_32bit = is_horizontal ? 0 : 1;

  ColorBuffer* input_color = is_horizontal ? &ambient_map_1_ : &ambient_map_0_;
  ColorBuffer* output_color = is_horizontal ? &ambient_map_0_ : &ambient_map_1_;
  
  context.SetRootSignature(ssao_root_signature_);
  context.SetPipelineState(blur_pso_);
  
  //context.SetViewports(&viewport_);
  //context.SetScissorRects(&scissor_rect_);

  context.TransitionResource(*output_color, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
  context.ClearColor(*output_color);
  context.SetRenderTarget(&output_color->Rtv());

  context.SetDynamicConstantBufferView(0, sizeof(SsaoConstants), &ssao_constants_);
  context.SetRoot32BitConstant(1, root_32bit, 0);

  D3D12_CPU_DESCRIPTOR_HANDLE handles[] = { normal_map_.Srv(), depth_buffer_.DepthSRV(), input_color->Srv() }; 
  context.SetDynamicDescriptors(2, 0, _countof(handles), handles);

  context.SetVertexBuffers(0, 0, nullptr);

  context.SetNullIndexBuffer();
  context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  context.DrawInstanced(6, 1, 0, 0);
  context.TransitionResource(*output_color, D3D12_RESOURCE_STATE_GENERIC_READ, true);
  //  context.Flush(true);
}

vector<float> Ssao::GetGaussWeight(float sigma) {
  float two_sigma_squr = 2.0f * sigma * sigma;
  int blur_radius = (int)ceil(sigma * 2.0f);
  std::vector<float> weights(blur_radius*2 + 1);

  float weight_sum = 0.0f;

  for (int i=-blur_radius; i<=blur_radius; ++i) {
    float x = (float)i;

    float weight = expf(-x*x/two_sigma_squr);
    weights[i+blur_radius] = weight;
    weight_sum += weight;
  }

  for (size_t i=0; i<weights.size(); ++i) {
    weights[i] /= weight_sum;
  }

  return weights;
}

void Ssao::BuildOffsetVectors()
{
  // Start with 14 uniformly distributed vectors.  We choose the 8 corners of the cube
  // and the 6 center points along each cube face.  We always alternate the points on 
  // opposites sides of the cubes.  This way we still get the vectors spread out even
  // if we choose to use less than 14 samples.

  // 8 cube corners
  offsets_[0] = XMFLOAT4(+1.0f, +1.0f, +1.0f, 0.0f);
  offsets_[1] = XMFLOAT4(-1.0f, -1.0f, -1.0f, 0.0f);

  offsets_[2] = XMFLOAT4(-1.0f, +1.0f, +1.0f, 0.0f);
  offsets_[3] = XMFLOAT4(+1.0f, -1.0f, -1.0f, 0.0f);

  offsets_[4] = XMFLOAT4(+1.0f, +1.0f, -1.0f, 0.0f);
  offsets_[5] = XMFLOAT4(-1.0f, -1.0f, +1.0f, 0.0f);

  offsets_[6] = XMFLOAT4(-1.0f, +1.0f, -1.0f, 0.0f);
  offsets_[7] = XMFLOAT4(+1.0f, -1.0f, +1.0f, 0.0f);

  // 6 centers of cube faces
  offsets_[8] = XMFLOAT4(-1.0f, 0.0f, 0.0f, 0.0f);
  offsets_[9] = XMFLOAT4(+1.0f, 0.0f, 0.0f, 0.0f);

  offsets_[10] = XMFLOAT4(0.0f, -1.0f, 0.0f, 0.0f);
  offsets_[11] = XMFLOAT4(0.0f, +1.0f, 0.0f, 0.0f);

  offsets_[12] = XMFLOAT4(0.0f, 0.0f, -1.0f, 0.0f);
  offsets_[13] = XMFLOAT4(0.0f, 0.0f, +1.0f, 0.0f);

  for(int i = 0; i < 14; ++i)
  {
    // Create random lengths in [0.25, 1.0].
    float s = MathHelper::RandF(0.25f, 1.0f);

    XMVECTOR v = s * XMVector4Normalize(XMLoadFloat4(&offsets_[i]));

    XMStoreFloat4(&offsets_[i], v);
  }
}

void Ssao::OnSetEnabled(bool enable) {
  GraphicsContext& context = GraphicsContext::Begin(L"ssao clearer");
  context.ClearColor(ambient_map_0_);
  context.ClearColor(ambient_map_1_);
  context.ClearDepth(depth_buffer_);
  context.ClearColor(normal_map_);
  context.Finish();
}

}

}

