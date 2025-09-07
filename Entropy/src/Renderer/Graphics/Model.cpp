#include "Model.h"

bool Model::Initialize(ID3D11Device* device, ID3D11DeviceContext* deviceContext, ID3D11ShaderResourceView* texture, ConstantBuffer<CB_VS_vertexshader>& cb_vs_vertexshader)
{
    this->device = device;
    this->deviceContext = deviceContext;
    this->texture = texture;
    this->cb_vs_vertexshader = &cb_vs_vertexshader;

    try
    {
        Vertex v[] =
        {
            Vertex(-0.5f, -0.5f, 0.0f, 0.0f, 1.0f),//BL 0
            Vertex(-0.5f, 0.5f, 0.0f, 0.0f, 0.0f),//TL  1
            Vertex(0.5f, 0.5f, 0.0f, 1.0f, 0.0f),//TR   2
            Vertex(0.5f, -0.5f, 0.0f, 1.0f, 1.0f)//BR   3

        };

        DWORD indices[] =
        {
            0,1,2,
            0,2,3
        };

        HRESULT hr = this->vertexBuffer.Initialize(device, v, ARRAYSIZE(v));
        COM_ERROR_IF_FAILED(hr, "Failed to initialize vertex buffer");

        //Load Index Buffer

        hr = indexBuffer.Initialize(device, indices, ARRAYSIZE(indices));
        COM_ERROR_IF_FAILED(hr, "Failed to initialize index buffer");
    }
    catch (COMException& exception)
    {
        ErrorLogger::Log(exception);
    }


    this->UpdateWorldMatrix();
    return true;

}


void Model::SetTexture(ID3D11ShaderResourceView* texture)
{
    this->texture = texture;
}

void Model::Draw(const XMMATRIX& viewProjectionMatrix)
{
    this->cb_vs_vertexshader->data.mat = this->worldMatrix * viewProjectionMatrix;
    this->cb_vs_vertexshader->data.mat = XMMatrixTranspose(this->cb_vs_vertexshader->data.mat);
    this->cb_vs_vertexshader->ApplyChanges();
    this->deviceContext->VSSetConstantBuffers(0, 1, this->cb_vs_vertexshader->GetAddressOf());

    this->deviceContext->PSSetShaderResources(0, 1, &this->texture);
    this->deviceContext->IASetIndexBuffer(this->indexBuffer.Get(), DXGI_FORMAT::DXGI_FORMAT_R32_UINT,0);
    UINT offset = 0;
    this->deviceContext->IASetVertexBuffers(0, 1, this->vertexBuffer.GetAddressOf(), this->vertexBuffer.StridePtr(), &offset);
    this->deviceContext->DrawIndexed(this->indexBuffer.BufferSize(), 0, 0);
}

void Model::UpdateWorldMatrix()
{
    this->worldMatrix = XMMatrixIdentity();
}