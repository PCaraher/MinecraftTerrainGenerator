//***************************************************************************************
// BlendApp.cpp by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

#include "Common/d3dApp.h"
#include "Common/MathHelper.h"
#include "Common/UploadBuffer.h"
#include "Common/GeometryGenerator.h"
#include "FrameResource.h"
#include "Waves.h"
#include <time.h>
#include "Camera.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

const int gNumFrameResources = 3;

// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
struct RenderItem
{
	RenderItem() = default;
 
    // World matrix of the shape that describes the object's local space
    // relative to the world space, which defines the position, orientation,
    // and scale of the object in the world.
    XMFLOAT4X4 World = MathHelper::Identity4x4();

	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

    // Primitive topology.
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // DrawIndexedInstanced parameters.
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;

	bool shouldRender = false;
	bool isShadow = false;
	bool isPlayer = false;
};

enum class RenderLayer : int
{
	Opaque = 0,
	Transparent,
	AlphaTested,
	Shadow,
	Count
};

class BlendApp : public D3DApp
{
public:
    BlendApp(HINSTANCE hInstance);
    BlendApp(const BlendApp& rhs) = delete;
    BlendApp& operator=(const BlendApp& rhs) = delete;
    ~BlendApp();

    virtual bool Initialize()override;

private:
    virtual void OnResize()override;
    virtual void Update(const GameTimer& gt)override;
    virtual void Draw(const GameTimer& gt)override;

    virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

    void OnKeyboardInput(const GameTimer& gt);
	void OnCharKeyboardinput(const GameTimer& gt);
	//void UpdateCamera(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);

	void LoadTextures();
    void BuildRootSignature();
	void BuildDescriptorHeaps();
    void BuildShadersAndInputLayout();
	void BuildBoxGeometry();
	void BuildSkyBoxGeometry();
    void BuildPSOs();
    void BuildFrameResources();
    void BuildMaterials();
	std::string Block(int y, int size);
    void BuildRenderItems(int worldsize);
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

	//Don't need
    float GetHillsHeight(float x, float z)const;
    //XMFLOAT3 GetHillsNormal(float x, float z)const;

private:

    std::vector<std::unique_ptr<FrameResource>> mFrameResources;
    FrameResource* mCurrFrameResource = nullptr;
    int mCurrFrameResourceIndex = 0;

    UINT mCbvSrvDescriptorSize = 0;

    ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
 
	//Don't need
    RenderItem* mWavesRitem = nullptr;
	RenderItem* mShadowedBoxRitem = nullptr; 

	RenderItem* mcharRitem = nullptr;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	std::vector<std::vector<std::vector<std::unique_ptr<RenderItem>>>> test;
	std::vector<std::unique_ptr<RenderItem>> ***test2;

	// Render items divided by PSO.
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

	//Don't need
	std::unique_ptr<Waves> mWaves;

	XMFLOAT3 mCharTranslation = { 0.0f,2.0f,0.0f };// The characters position
	XMFLOAT3 altCameraPos = { 0.0f,25.0f,0.0f }; //The top down cameras position

    PassConstants mMainPassCB;

	bool mIsWireframe = false; //Value that changes how the program draws the blocks
	bool change = false; //Value that changes the camera mode


	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

    float mTheta = 1.5f*XM_PI;
    float mPhi = XM_PIDIV2 - 0.1f;
    float mRadius = 50.0f;

    POINT mLastMousePos;
	Camera mCamera;
	
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
    PSTR cmdLine, int showCmd)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    try
    {
        BlendApp theApp(hInstance);
        if(!theApp.Initialize())
            return 0;

        return theApp.Run();
    }
    catch(DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}

BlendApp::BlendApp(HINSTANCE hInstance)
    : D3DApp(hInstance)
{
}

BlendApp::~BlendApp()
{
    if(md3dDevice != nullptr)
        FlushCommandQueue();
}

bool BlendApp::Initialize()
{
    if(!D3DApp::Initialize())
        return false;

    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    // Get the increment size of a descriptor in this heap type.  This is hardware specific, 
	// so we have to query this information.
    mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    mWaves = std::make_unique<Waves>(128, 128, 1.0f, 0.03f, 4.0f, 0.2f);
	mCamera.SetPosition(0.0f, 2.0f, -15.0f);
 
	LoadTextures();
    BuildRootSignature();
	BuildDescriptorHeaps();
    BuildShadersAndInputLayout();
	BuildBoxGeometry();
	BuildSkyBoxGeometry(); 
	BuildMaterials();
    BuildRenderItems(32); //Parameter determines the size of the terrain e.g size x size. The depth of the terrain is hard coded 
    BuildFrameResources();
    BuildPSOs();
	

    // Execute the initialization commands.
    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Wait until initialization is complete.
    FlushCommandQueue();

    return true;
}
 
void BlendApp::OnResize()
{
    D3DApp::OnResize();

    // The window resized, so update the aspect ratio and recompute the projection matrix.
    XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&mProj, P);
}

void BlendApp::Update(const GameTimer& gt)
{
	OnCharKeyboardinput(gt);//Checks for player input and updates the position
    OnKeyboardInput(gt);
	//UpdateCamera(gt);

    // Cycle through the circular frame resource array.
    mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
    mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

    // Has the GPU finished processing the commands of the current frame resource?
    // If not, wait until the GPU has completed commands up to this fence point.
    if(mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

	UpdateObjectCBs(gt);
	UpdateMaterialCBs(gt);
	UpdateMainPassCB(gt);
}

void BlendApp::Draw(const GameTimer& gt)
{
    auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
    ThrowIfFailed(cmdListAlloc->Reset());

    // A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
    //ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));
	if (mIsWireframe)
	{
		ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque_wireframe"].Get()));
	}
	else
	{
		ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));
	}

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    // Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    // Clear the back buffer and depth buffer.
    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), (float*)&mMainPassCB.FogColor, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // Specify the buffers we are going to render to.
    mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

    DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

	mCommandList->SetPipelineState(mPSOs["alphaTested"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::AlphaTested]);

	mCommandList->OMSetStencilRef(0);

	mCommandList->SetPipelineState(mPSOs["transparent"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Transparent]);


	//Draw shadows
	//omstencil was here
	mCommandList->SetPipelineState(mPSOs["shadow"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Shadow]);

    // Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // Done recording commands.
    ThrowIfFailed(mCommandList->Close());

    // Add the command list to the queue for execution.
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Swap the back and front buffers
    ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    // Advance the fence value to mark commands up to this fence point.
    mCurrFrameResource->Fence = ++mCurrentFence;


    // Add an instruction to the command queue to set a new fence point. 
    // Because we are on the GPU timeline, the new fence point won't be 
    // set until the GPU finishes processing all the commands prior to this Signal().
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void BlendApp::OnMouseDown(WPARAM btnState, int x, int y) //clicking on the mouse button allows the player to move the camera
{
    mLastMousePos.x = x;
    mLastMousePos.y = y;

    SetCapture(mhMainWnd);
}

void BlendApp::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void BlendApp::OnMouseMove(WPARAM btnState, int x, int y) //this is is where we rotate the camera's look direction
{
    if((btnState & MK_LBUTTON) != 0)
    {
        // Make each pixel correspond to a quarter of a degree.
        float dx = XMConvertToRadians(0.25f*static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(0.25f*static_cast<float>(y - mLastMousePos.y));

    //    // Update angles based on input to orbit camera around box.
    //    mTheta += dx;
    //    mPhi += dy;

    //    // Restrict the angle mPhi.
    //    mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
    //}
    //else if((btnState & MK_RBUTTON) != 0)
    //{
    //    // Make each pixel correspond to 0.2 unit in the scene.
    //    float dx = 0.2f*static_cast<float>(x - mLastMousePos.x);
    //    float dy = 0.2f*static_cast<float>(y - mLastMousePos.y);

    //    // Update the camera radius based on input.
    //    mRadius += dx - dy;

    //    // Restrict the radius.
    //    mRadius = MathHelper::Clamp(mRadius, 5.0f, 150.0f);

		mCamera.Pitch(dy);
		mCamera.RotateY(dx);
    }

    mLastMousePos.x = x;
    mLastMousePos.y = y;
}

//Function to change the characters translation by using UHJK keys
void BlendApp::OnCharKeyboardinput(const GameTimer& gt)
{
	const float dt = gt.DeltaTime();

	if (GetAsyncKeyState('H') & 0x8000)
		mCharTranslation.x -= 4.0f*dt;
	if (GetAsyncKeyState('K') & 0x8000)
		mCharTranslation.x += 4.0f*dt;
	if (GetAsyncKeyState('U') & 0x8000)
		mCharTranslation.z += 4.0f*dt;
	if (GetAsyncKeyState('J') & 0x8000)
		mCharTranslation.z -= 4.0f*dt;

	//Code to restrict the players range of movement
	mCharTranslation.z = MathHelper::Max(mCharTranslation.z, -8.0f);
	mCharTranslation.x = MathHelper::Max(mCharTranslation.x, -10.0f);

}
 
void BlendApp::OnKeyboardInput(const GameTimer& gt)
{
	const float dt = gt.DeltaTime();
	
	//The user can toggle between wireframe and solid modes using the 1 and 2 keys
	if (GetAsyncKeyState('1') & 0x8000)
		mIsWireframe = true;
	else if(GetAsyncKeyState('2')& 0x8000)
		mIsWireframe = false;

	//This is the alternate camera mode ie the top down camera view
	if (change == true)
	{
		mCamera.SetPosition(0.0f, 25.0f, 0.0f);//Sets the cameras position above the map
		mCamera.SetLens(0.25f*MathHelper::Pi, 1.0f, 1.0f, 1000.0f);
		XMFLOAT3 altPos = { 0.0f,80.5f,0.0f };
		XMFLOAT3 altTarget = { 0.0f,0.5f,0.0f };
		XMFLOAT3 altWorldUp = { 0.5f,0.0f,0.0f };
		mCamera.LookAt(altPos, altTarget, altWorldUp);//Locks the cameras view so it looks down on the map
	}
	if (change == false) {
		//Code to change the position of our free camera using the WASD keys
		if (GetAsyncKeyState('W') & 0x8000)
			mCamera.Walk(10.0f*dt);

		else if (GetAsyncKeyState('S') & 0x8000)
			mCamera.Walk(-10.0f*dt);

		else if (GetAsyncKeyState('A') & 0x8000)
			mCamera.Strafe(-10.0f*dt);

		else if (GetAsyncKeyState('D') & 0x8000)
			mCamera.Strafe(10.0f*dt);
	}

	if (GetAsyncKeyState('3') & 0x8000) {
		if (change == false)
		{
			change = true;
		}
	}
	if (GetAsyncKeyState('4') & 0x8000) {
		if (change == true)
		{
			change = false;
		}
	}

	mCamera.UpdateViewMatrix();
}
 
//void BlendApp::UpdateCamera(const GameTimer& gt)
//{
//	// Convert Spherical to Cartesian coordinates.
//	mEyePos.x = mRadius*sinf(mPhi)*cosf(mTheta);
//	mEyePos.z = mRadius*sinf(mPhi)*sinf(mTheta);
//	mEyePos.y = mRadius*cosf(mPhi);
//
//	// Build the view matrix.
//	XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
//	XMVECTOR target = XMVectorZero();
//	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
//
//	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
//	XMStoreFloat4x4(&mView, view);
//}



void BlendApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for(auto& e : mAllRitems)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if(e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);
			

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}

		//This is where the shadows are supposed to be updated so they look like they're changing over time
		//Currently it works for the only shadow that has been rendered.
		if (e->isShadow)
		{
			// Update shadow world matrix.
			XMVECTOR shadowPlane = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f); // xz plane
			XMVECTOR toMainLight = -XMLoadFloat3(&mMainPassCB.Lights[0].Direction);
			XMMATRIX S = XMMatrixShadow(shadowPlane, toMainLight);
			XMMATRIX shadowOffsetY = XMMatrixTranslation(0.0f, 0.001f, 0.0f);
			XMStoreFloat4x4(&e->World, S * shadowOffsetY);

			//currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			e->NumFramesDirty = gNumFrameResources;
		}

		//Code to update the characters position using the chartranslation value
		if (e->isPlayer)
		{
			XMMATRIX player = XMLoadFloat4x4(&e->World);
			XMMATRIX update = XMMatrixTranslation(mCharTranslation.x, mCharTranslation.y, mCharTranslation.z);

			player = update*player;

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(player));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);
		}
	}
}

void BlendApp::UpdateMaterialCBs(const GameTimer& gt)
{
	auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
	for(auto& e : mMaterials)
	{
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if(mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstants matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0 = mat->FresnelR0;
			matConstants.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));

			currMaterialCB->CopyData(mat->MatCBIndex, matConstants);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

void BlendApp::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = mCamera.GetView();
	XMMATRIX proj = mCamera.GetProj();

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);


	//Use the cosine of time passed to get a float between 1 and -1.
	//Use that cosine function to change the direction our light is coming from over time.
	//Also use the cosine function to change the intensity of the light in the world over time, to give the illusion of 
	//day and night
	float rotation = gt.TotalTime();
	float xCoord = 0.5f*(cos(0.05f*rotation));
	float strength = xCoord;


	//Currently aren't using this block of code. t was originally intended to be used to change the intensity 
	//Of the light whenever the directional lights x coordinate reaches the max/min value it can
	//if (xCoord <= -0.45f || xCoord >= +0.45f)
	//{
	//	if (xCoord <= -0.45f)
	//		test -= 0.1f;
	//	else if (xCoord >= 0.45f)
	//		test += 0.1f;
	//	//test *=-1;//=0.0f;
	//}

	//This code was originally intended to change the y coordinate of the light source so that the source would move 
	//in a cicular motion around the z axis
	//float yCoord = sqrt(powf(xCoord,2.0f) - 100.0f);
	

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mMainPassCB.EyePosW = mCamera.GetPosition3f();
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();
	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	mMainPassCB.Lights[0].Direction = { xCoord, -0.57735f, 0.57735f };// Changing this lights direction vector using the xCoord variable.
	mMainPassCB.Lights[0].Strength = { strength, strength, strength }; //Was {0.9f, 0.9f, 0.8f} The strength value is the same as the xCoord, it will change intensity over time
	mMainPassCB.Lights[1].Direction = { 0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[1].Strength = { strength, strength, strength };
	mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.Lights[2].Strength = { strength, strength, strength };

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void BlendApp::LoadTextures()
{

	auto grassTex = std::make_unique<Texture>(); //We are using this texture to create the sky as this is the first texture it loads from
	grassTex->Name = "grassTex";
	grassTex->Filename = L"Textures/SkyBox.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), grassTex->Filename.c_str(),
		grassTex->Resource, grassTex->UploadHeap));

	auto waterTex = std::make_unique<Texture>();
	waterTex->Name = "waterTex";
	waterTex->Filename = L"Textures/water1.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), waterTex->Filename.c_str(),
		waterTex->Resource, waterTex->UploadHeap));

	auto fenceTex = std::make_unique<Texture>();
	fenceTex->Name = "fenceTex";
	fenceTex->Filename = L"Textures/WireFence.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), fenceTex->Filename.c_str(),
		fenceTex->Resource, fenceTex->UploadHeap));

	auto grass1Tex = std::make_unique<Texture>();
	grass1Tex->Name = "grass1Tex";
	grass1Tex->Filename = L"Textures/grass1.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), grass1Tex->Filename.c_str(),
		grass1Tex->Resource, grass1Tex->UploadHeap));


	

	//Need to get rid of the above textures
	//Minecraft textures

	auto mGrass = std::make_unique<Texture>();
	mGrass->Name = "mGrass";
	mGrass->Filename = L"Textures/grassFull.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), mGrass->Filename.c_str(),
		mGrass->Resource, mGrass->UploadHeap));

	auto mDirt = std::make_unique<Texture>();
	mDirt->Name = "mDirt";
	mDirt->Filename = L"Textures/dirt.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), mDirt->Filename.c_str(),
		mDirt->Resource, mDirt->UploadHeap));

	auto mStone = std::make_unique<Texture>();
	mStone->Name = "mStone";
	mStone->Filename = L"Textures/stone.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), mStone->Filename.c_str(),
		mStone->Resource, mStone->UploadHeap));

	auto mBedRock = std::make_unique<Texture>();
	mBedRock->Name = "mBedRock";
	mBedRock->Filename = L"Textures/bedrock.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), mBedRock->Filename.c_str(),
		mBedRock->Resource, mBedRock->UploadHeap));

	auto mEmerald = std::make_unique<Texture>();
	mEmerald->Name = "mEmerald";
	mEmerald->Filename = L"Textures/emerald0.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), mEmerald->Filename.c_str(),
		mEmerald->Resource, mEmerald->UploadHeap));

	
	mTextures[grassTex->Name] = std::move(grassTex);
	mTextures[waterTex->Name] = std::move(waterTex);
	mTextures[fenceTex->Name] = std::move(fenceTex);
	mTextures[grass1Tex->Name] = std::move(grass1Tex);
	//minecraft textures
	mTextures[mGrass->Name] = std::move(mGrass);
	mTextures[mDirt->Name] = std::move(mDirt);
	mTextures[mStone->Name] = std::move(mStone);
	mTextures[mBedRock->Name] = std::move(mBedRock);
	mTextures[mEmerald->Name] = std::move(mEmerald);
	
}

void BlendApp::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
    slotRootParameter[1].InitAsConstantBufferView(0);
    slotRootParameter[2].InitAsConstantBufferView(1);
    slotRootParameter[3].InitAsConstantBufferView(2);

	auto staticSamplers = GetStaticSamplers();

    // A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

    if(errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void BlendApp::BuildDescriptorHeaps()
{
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 9;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	auto grassTex = mTextures["grassTex"]->Resource;
	auto waterTex = mTextures["waterTex"]->Resource;
	auto fenceTex = mTextures["fenceTex"]->Resource;
	auto grass1Tex = mTextures["grass1Tex"]->Resource;
	//Don't need the above
	//Minecraft
	auto mGrass = mTextures["mGrass"]->Resource;
	auto mDirt = mTextures["mDirt"]->Resource;
	auto mStone = mTextures["mStone"]->Resource;
	auto mBedRock = mTextures["mBedRock"]->Resource;
	auto mEmerald = mTextures["mEmerald"]->Resource;
	


	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = grassTex->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = -1;
	md3dDevice->CreateShaderResourceView(grassTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = waterTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(waterTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = fenceTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(fenceTex.Get(), &srvDesc, hDescriptor);

	//next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = grass1Tex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(grass1Tex.Get(), &srvDesc, hDescriptor);
	
	//
	//*minecraft descriptors*
	//

	//next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = mGrass->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(mGrass.Get(), &srvDesc, hDescriptor);

	//next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = mDirt->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(mDirt.Get(), &srvDesc, hDescriptor);

	//next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = mStone->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(mStone.Get(), &srvDesc, hDescriptor);

	//next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = mBedRock->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(mBedRock.Get(), &srvDesc, hDescriptor);

	//next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = mEmerald->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(mEmerald.Get(), &srvDesc, hDescriptor);



}

void BlendApp::BuildShadersAndInputLayout()
{
	const D3D_SHADER_MACRO defines[] =
	{
		"FOG", "1",
		NULL, NULL
	};

	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"FOG", "1",
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", defines, "PS", "ps_5_0");
	mShaders["alphaTestedPS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", alphaTestDefines, "PS", "ps_5_0");
	
    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
}


void BlendApp::BuildBoxGeometry()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 0); 

	std::vector<Vertex> vertices(box.Vertices.size());
	for (size_t i = 0; i < box.Vertices.size(); ++i)
	{
		auto& p = box.Vertices[i].Position;
		vertices[i].Pos = p;
		vertices[i].Normal = box.Vertices[i].Normal;
		vertices[i].TexC = box.Vertices[i].TexC;
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

	std::vector<std::uint16_t> indices = box.GetIndices16();
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "boxGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["box"] = submesh;

	mGeometries["boxGeo"] = std::move(geo);
}

void BlendApp::BuildSkyBoxGeometry() //this is where the sky box is built and creates a geometry
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData SkyBox = geoGen.CreateSkyBox(1.0f, 1.0f, 1.0f, 0);

	std::vector<Vertex> vertices(SkyBox.Vertices.size());
	for (size_t i = 0; i < SkyBox.Vertices.size(); ++i)
	{
		auto& p = SkyBox.Vertices[i].Position;
		vertices[i].Pos = p;
		vertices[i].Normal = SkyBox.Vertices[i].Normal;
		vertices[i].TexC = SkyBox.Vertices[i].TexC;
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

	std::vector<std::uint16_t> indices = SkyBox.GetIndices16();
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "skyboxGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["skybox"] = submesh;

	mGeometries["skyboxGeo"] = std::move(geo);
}

void BlendApp::BuildPSOs()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	//
	// PSO for opaque objects.
	//
    ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS = 
	{ 
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()), 
		mShaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS = 
	{ 
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};

	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	//opaquePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
	opaquePsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));


	//
	//PSO for wireframe objects
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
	opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&mPSOs["opaque_wireframe"])));


	//
	//PSO for transparent objects
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentPsoDesc = opaquePsoDesc;

	D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
	transparencyBlendDesc.BlendEnable = true;
	transparencyBlendDesc.LogicOpEnable = false;
	transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	transparencyBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	transparentPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&transparentPsoDesc, 
		IID_PPV_ARGS(&mPSOs["transparent"])));


	//
	// PSO for alpha tested objects
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC alphaTestedPsoDesc = opaquePsoDesc;
	alphaTestedPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["alphaTestedPS"]->GetBufferPointer()),
		mShaders["alphaTestedPS"]->GetBufferSize()
	};
	//Disable back face culling for Alpha tested objects
	alphaTestedPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&alphaTestedPsoDesc, IID_PPV_ARGS(&mPSOs["alphaTested"])));

	//
	// PSO for shadow objects
	//

	//We are going to draw shadows with transparency, so base it off the transparency description.
	D3D12_DEPTH_STENCIL_DESC shadowDSS;
	shadowDSS.DepthEnable = true;
	shadowDSS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	shadowDSS.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	shadowDSS.StencilEnable = true;
	shadowDSS.StencilReadMask = 0xff;
	shadowDSS.StencilWriteMask = 0xff;

	shadowDSS.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	shadowDSS.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	shadowDSS.FrontFace.StencilPassOp = D3D12_STENCIL_OP_INCR;
	shadowDSS.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	//We are not rendering backfacing polygons, so these settings do not matter.
	shadowDSS.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	shadowDSS.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	shadowDSS.BackFace.StencilPassOp = D3D12_STENCIL_OP_INCR;
	shadowDSS.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowPsoDesc = transparentPsoDesc;
	shadowPsoDesc.DepthStencilState = shadowDSS;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&shadowPsoDesc,
		IID_PPV_ARGS(&mPSOs["shadow"])));

}//End Build PSOs

void BlendApp::BuildFrameResources()
{
    for(int i = 0; i < gNumFrameResources; ++i)
    {
        mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
            1, (UINT)mAllRitems.size(), (UINT)mMaterials.size(), mWaves->VertexCount()));
    }
}

void BlendApp::BuildMaterials()
{
	auto grass = std::make_unique<Material>();
	grass->Name = "grass";
	grass->MatCBIndex = 0;
	grass->DiffuseSrvHeapIndex = 0;
	grass->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	grass->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	grass->Roughness = 0.125f;

	auto water = std::make_unique<Material>();
	water->Name = "water";
	water->MatCBIndex = 1;
	water->DiffuseSrvHeapIndex = 1;
	water->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.5f);
	water->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	water->Roughness = 0.0f;

	auto wirefence = std::make_unique<Material>();
	wirefence->Name = "wirefence";
	wirefence->MatCBIndex = 2;
	wirefence->DiffuseSrvHeapIndex = 2;
	wirefence->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	wirefence->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	wirefence->Roughness = 0.25f;

	auto grass1 = std::make_unique<Material>();
	grass1->Name = "grass1";
	grass1->MatCBIndex = 3;
	grass1->DiffuseSrvHeapIndex = 3;
	grass1->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	grass1->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	grass1->Roughness = 0.25f;


	//Need to get rid of above materials
	//Minecraft materials
	auto mGrass = std::make_unique<Material>();
	mGrass->Name = "mGrass";
	mGrass->MatCBIndex = 4;
	mGrass->DiffuseSrvHeapIndex = 4;
	mGrass->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	mGrass->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	mGrass->Roughness = 0.125f;
	
	auto mDirt = std::make_unique<Material>();
	mDirt->Name = "mDirt";
	mDirt->MatCBIndex = 5;
	mDirt->DiffuseSrvHeapIndex = 5;
	mDirt->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	mDirt->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	mDirt->Roughness = 0.25f;

	auto mStone = std::make_unique<Material>();
	mStone->Name = "mStone";
	mStone->MatCBIndex = 6;
	mStone->DiffuseSrvHeapIndex = 6;
	mStone->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	mStone->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	mStone->Roughness = 0.25f;

	auto mBedRock = std::make_unique<Material>();
	mBedRock->Name = "mBedRock";
	mBedRock->MatCBIndex = 7;
	mBedRock->DiffuseSrvHeapIndex = 7;
	mBedRock->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	mBedRock->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	mBedRock->Roughness = 0.25f;

	auto mEmerald = std::make_unique<Material>();
	mEmerald->Name = "mEmerald";
	mEmerald->MatCBIndex = 8;
	mEmerald->DiffuseSrvHeapIndex = 8;
	mEmerald->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	mEmerald->FresnelR0 = XMFLOAT3(0.1f, 0.1f,0.1f);
	mEmerald->Roughness = 0.25f;

	//This is the material for the planar shadow that didn't enitirely work.
	//Only one shadow was rendered, but the material was applied successfully
	auto shadowMat = std::make_unique<Material>();
	shadowMat->Name = "shadowMat";
	shadowMat->MatCBIndex = 9;
	shadowMat->DiffuseSrvHeapIndex = 4;
	shadowMat->DiffuseAlbedo = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.5f);
	shadowMat->FresnelR0 = XMFLOAT3(0.001f, 0.001f, 0.001f);
	shadowMat->Roughness = 0.0f;


	mMaterials["grass"] = std::move(grass);
	mMaterials["water"] = std::move(water);
	mMaterials["wirefence"] = std::move(wirefence);
	mMaterials["grass1"] = std::move(grass1);
	//Don't need the materials above
	mMaterials["mGrass"] = std::move(mGrass);
	mMaterials["mDirt"] = std::move(mDirt);
	mMaterials["mStone"] = std::move(mStone);
	mMaterials["mBedRock"] = std::move(mBedRock);
	mMaterials["mEmerald"] = std::move(mEmerald);
	mMaterials["shadowMat"] = std::move(shadowMat);
}


//Method to return the material type at the specified level y
//Also randomly places emerald ore among the stone levels
std::string BlendApp::Block(int y, int size)
{
	bool tf = (rand() % 2) != 0; //Generates a random number between 0 and 1.
	bool emerald = (rand() % 20 == 0); //Creates a 1 in 20 chance of placing an emerald textured block
	


	if (y < 2)	//Bedrock layer
		return "mBedRock";
	else if (y > 2 && y < (size/2))	//Stone and emerald layer
	{
		if (emerald)
			return "mEmerald";
		else
			return "mStone";
	}
	else if (y > (size/2) && y < (size-2))	//Dirt layer
		return "mDirt";
	else if (y>=(size-2) && y<=size)	//Grass layer
		return "mGrass";

	//Checks the borders of bedrock/stone and stone/dirt and makes it look like they blend
	//into each other. Using the tf bool it will be a 50/50 chance for the block on the border
	//to be either one texture or the other
	else if (y == 2)
	{
		if (tf)
			return "mBedRock";
		else
			return "mStone";
	}
	else if (y == size/2)
	{
		if (tf)
			return "mStone";
		else
			return "mDirt";
	}
	return "mGrass";
}

void BlendApp::BuildRenderItems(int worldsize)
{
	srand (time(NULL));	
	int size = worldsize;
	const int DEPTH = 8;
	int i = 0;


	//Character render item code
	auto charRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&charRitem->World, XMMatrixTranslation(mCharTranslation.x, mCharTranslation.y, mCharTranslation.z));//Translates the character to the mchartranslation float3
	charRitem->ObjCBIndex = i;
	charRitem->Mat = mMaterials["wirefence"].get();
	charRitem->Geo = mGeometries["boxGeo"].get();
	charRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	charRitem->IndexCount = charRitem->Geo->DrawArgs["box"].IndexCount;
	charRitem->StartIndexLocation = charRitem->Geo->DrawArgs["box"].StartIndexLocation;
	charRitem->BaseVertexLocation = charRitem->Geo->DrawArgs["box"].BaseVertexLocation;
	charRitem->shouldRender = true;
	charRitem->isPlayer = true;
	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(charRitem.get());
	mAllRitems.push_back(std::move(charRitem));

	//Skybox creation and size
	auto skyboxRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&skyboxRitem->World, XMMatrixScaling(200, 200, 200));
	skyboxRitem->ObjCBIndex = ++i;
	skyboxRitem->Mat = mMaterials["grass"].get(); //loads first material called grass but is the skybox.dds
	skyboxRitem->Geo = mGeometries["skyboxGeo"].get();
	skyboxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skyboxRitem->IndexCount = skyboxRitem->Geo->DrawArgs["skybox"].IndexCount;
	skyboxRitem->StartIndexLocation = skyboxRitem->Geo->DrawArgs["skybox"].StartIndexLocation;
	skyboxRitem->BaseVertexLocation = skyboxRitem->Geo->DrawArgs["skybox"].BaseVertexLocation;
	skyboxRitem->shouldRender = true;
	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(skyboxRitem.get());
	mAllRitems.push_back(std::move(skyboxRitem));


	//This is the terrain generator. 
	//Goes through each x,z coordinate and builds up the y direction
	//So at x=0 and z=0 it will build a stack of render items to a random height between DEPTH -1 and DEPTH -3
	for (int x = 0; x < size; x++) 
	{
		for (int z = 0; z < size; z++)
		{
			for (int y = 0; y < rand() % 2 + (DEPTH - 1); y++)
			{
				auto boxRitem = std::make_unique<RenderItem>();		
				XMStoreFloat4x4(&boxRitem->World, XMMatrixTranslation((float)x - (size / 2), (float)y - (DEPTH / 2), (float)z - (size / 2)));
				boxRitem->ObjCBIndex = ++i;
				boxRitem->Mat = mMaterials[Block(y, DEPTH)].get(); //Calls the Block method to determine the material of the block at position y
				boxRitem->Geo = mGeometries["boxGeo"].get();
				boxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
				boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
				boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
				boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
				boxRitem->shouldRender = false;						//We set the value of shouldRender to false so all of the render items won't render
																	//unless we specify that they should
				mRitemLayer[(int)RenderLayer::Opaque].push_back(boxRitem.get());

				if (x == 0 || y == 0 || z == 0 || x == size-1 || y >= DEPTH-4 || z == size-1) //Here, if the item is not within these specified borders
				{																			// then we don't render it
					boxRitem->shouldRender = true;
				}
				
				//If the current height is the top layer of the terrain then we're going to build a shadow item for the current item
				//This code currently only creates one planar shadow at the world origin
				//It does update with time and the position of the light source.
				if (y >= DEPTH-1)
				{
					auto shadowedBoxRitem = std::make_unique<RenderItem>();
					XMStoreFloat4x4(&shadowedBoxRitem->World, XMMatrixTranslation((float)x - (size / 2), (float)y - (DEPTH / 2), (float)z - (size / 2)));
					shadowedBoxRitem->ObjCBIndex = ++i;
					shadowedBoxRitem->Mat = mMaterials["shadowMat"].get();
					shadowedBoxRitem->Geo = mGeometries["boxGeo"].get();
					shadowedBoxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
					shadowedBoxRitem->IndexCount = shadowedBoxRitem->Geo->DrawArgs["box"].IndexCount;
					shadowedBoxRitem->StartIndexLocation = shadowedBoxRitem->Geo->DrawArgs["box"].StartIndexLocation;
					shadowedBoxRitem->BaseVertexLocation = shadowedBoxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
					shadowedBoxRitem->isShadow = true;
					shadowedBoxRitem->shouldRender = true;
					mRitemLayer[(int)RenderLayer::Shadow].push_back(shadowedBoxRitem.get());

					mAllRitems.push_back(std::move(shadowedBoxRitem));
				}
				mAllRitems.push_back(std::move(boxRitem));
			}//End y for			
		}//End z for
	}//End x for


}

void BlendApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	/*For destroying blocks, if we get on to it, could check the y coord of the destroyed and change all
	the shouldRender bools of the y-1 render items*/

    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto matCB = mCurrFrameResource->MaterialCB->Resource();

    // For each render item...
    for(size_t i = 0; i < ritems.size(); ++i)
    {
        auto ri = ritems[i];

		if (ri->shouldRender) //Checks if it should draw the item from ritems list
		{
			cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
			cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
			cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

			CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
			tex.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);

			D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex*objCBByteSize;
			D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex*matCBByteSize;

			cmdList->SetGraphicsRootDescriptorTable(0, tex);
			cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
			cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);

			cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
		}
        
    }
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> BlendApp::GetStaticSamplers()
{ 

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return { 
		pointWrap, pointClamp,
		linearWrap, linearClamp, 
		anisotropicWrap, anisotropicClamp };
}

float BlendApp::GetHillsHeight(float x, float z)const
{
    return 0.3f*(z*sinf(0.1f*x) + x*cosf(0.1f*z));
}
//
//XMFLOAT3 BlendApp::GetHillsNormal(float x, float z)const
//{
//    // n = (-df/dx, 1, -df/dz)
//    XMFLOAT3 n(
//        -0.03f*z*cosf(0.1f*x) - 0.3f*cosf(0.1f*z),
//        1.0f,
//        -0.3f*sinf(0.1f*x) + 0.03f*x*sinf(0.1f*z));
//
//    XMVECTOR unitNormal = XMVector3Normalize(XMLoadFloat3(&n));
//    XMStoreFloat3(&n, unitNormal);
//
//    return n;
//}
