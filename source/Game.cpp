//
// Game.cpp
//

#include "pch.h"
#include "Game.h"


//toreorganise
#include <fstream>

extern void ExitGame();

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

Game::Game() noexcept(false) 
{
    m_deviceResources = std::make_unique<DX::DeviceResources>();
    m_deviceResources->RegisterDeviceNotify(this);
}

Game::~Game()
{
	if (m_audEngine)
	{
		m_audEngine->Suspend();
	}
	m_eyeLoop.reset();
	m_spaceLoop.reset();
}

// Initialize the Direct3D resources required to run.
void Game::Initialize(HWND window, int width, int height)
{

	m_input.Initialise(window);

    m_deviceResources->SetWindow(window, width, height);

    m_deviceResources->CreateDeviceResources();
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

	m_fullscreenRect.left = 0;
	m_fullscreenRect.top = 0;
	m_fullscreenRect.right = 800;
	m_fullscreenRect.bottom = 600;

	m_CameraViewRect.left = 500;
	m_CameraViewRect.top = 0;
	m_CameraViewRect.right = 800;
	m_CameraViewRect.bottom = 240;

	//setup light
	m_Light.setAmbientColour(0.8f, 0.8f, 0.8f, 1.0f);
	m_Light.setDiffuseColour(1.0f, 1.0f, 1.0f, 1.0f);
	m_Light.setPosition(2.0f, 1.0f, 1.0f);
	m_Light.setDirection(-1.0f, -1.0f, 0.0f);

	//setup camera
	m_Camera01.setPosition(Vector3(0.0f, 0.0f, 4.0f));
	m_Camera01.setRotation(Vector3(-90.0f, 0.0f, 0.0f));	//orientation is -90 becuase zero will be looking up at the sky straight up. 

AUDIO_ENGINE_FLAGS eflags = AudioEngine_Default;
#ifdef _DEBUG
	eflags = eflags | AudioEngine_Debug;
#endif
	m_audEngine = std::make_unique<AudioEngine>(eflags);
	m_retryDefault = false;
	
	m_listener.SetPosition(Vector3(0.0f, 0.0f, 4.0f));
	m_eyeEmitter.SetPosition(Vector3(0.0f, 0.0f, 0.0f));
	m_spaceEmitter.SetPosition(Vector3(3.f, 2.f, -3.f));

	//Sound
	m_soundEye = std::make_unique<SoundEffect>(m_audEngine.get(), L"eye.wav");
	m_soundStellar = std::make_unique<SoundEffect>(m_audEngine.get(), L"space.wav");

	m_eyeLoop = m_soundEye->CreateInstance(SoundEffectInstance_Use3D);
	m_spaceLoop = m_soundStellar->CreateInstance(SoundEffectInstance_Use3D);

	m_eyeEmitter.ChannelCount = m_soundEye->GetFormat()->nChannels;
	m_spaceEmitter.ChannelCount = m_soundStellar->GetFormat()->nChannels;

	m_eyeLoop->Apply3D(m_listener, m_eyeEmitter);
	m_spaceLoop->Apply3D(m_listener, m_spaceEmitter);

	m_eyeLoop->Play(true);
	m_spaceLoop->Play(true);
}

#pragma region Frame Update
// Executes the basic game loop.
void Game::Tick()
{
	//take in input
	m_input.Update();								//update the hardware
	m_gameInputCommands = m_input.getGameInput();	//retrieve the input for our game
	
	//Update all game objects
    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

	//Render all game content. 
    Render();	
}

// Updates the world.
void Game::Update(DX::StepTimer const& timer)
{
	double d_time = m_timer.GetElapsedSeconds();
	//note that currently.  Delta-time is not considered in the game object movement. 
	
	if (m_gameInputCommands.left)
	{
		Vector3 position = m_Camera01.getPosition(); //get the position
		position -= (m_Camera01.getRight() * m_Camera01.getMoveSpeed()); //add the forward vector
		m_Camera01.setPosition(position);

	}
	if (m_gameInputCommands.right)
	{
		Vector3 position = m_Camera01.getPosition(); //get the position
		position += (m_Camera01.getRight() * m_Camera01.getMoveSpeed()); //add the forward vector
		m_Camera01.setPosition(position);

	}
	if (m_gameInputCommands.forward)
	{
		Vector3 position = m_Camera01.getPosition(); //get the position
		position += (m_Camera01.getForward()*m_Camera01.getMoveSpeed()); //add the forward vector
		m_Camera01.setPosition(position);

	}
	if (m_gameInputCommands.back)
	{
		Vector3 position = m_Camera01.getPosition(); //get the position
		position -= (m_Camera01.getForward()*m_Camera01.getMoveSpeed()); //add the forward vector
		m_Camera01.setPosition(position);
		
	}
	if (m_gameInputCommands.mouseRotate)
	{
		Vector3 rotation = m_Camera01.getRotation();
		rotation.y -= m_Camera01.getRotationSpeed() * d_time * m_gameInputCommands.mouseRotateX;
		rotation.x -= m_Camera01.getRotationSpeed() * d_time * m_gameInputCommands.mouseRotateY;
		m_Camera01.setRotation(rotation);

		Vector3 rotation_render = m_RenderToTextureCamera.getRotation(); //get the position
		rotation_render.y -= m_RenderToTextureCamera.getRotationSpeed() * d_time * m_gameInputCommands.mouseRotateX;
		rotation_render.x -= m_RenderToTextureCamera.getRotationSpeed() * d_time * m_gameInputCommands.mouseRotateY;
		m_RenderToTextureCamera.setRotation(rotation_render);
	}

	m_Camera01.Update();	//camera update.

	m_SnowSystem.Frame(d_time, m_deviceResources->GetD3DDeviceContext());
	m_FireSystem.Frame(d_time, m_deviceResources->GetD3DDeviceContext());
	m_SpaceSystem.Frame(d_time, m_deviceResources->GetD3DDeviceContext());

	m_view = m_Camera01.getCameraMatrix();
	m_world = Matrix::Identity;

	m_listener.Update(m_Camera01.getPosition(), DirectX::SimpleMath::Vector3::UnitY, d_time);
	if (m_eyeLoop)
	{
		m_eyeLoop->Apply3D(m_listener, m_eyeEmitter);
	}if (m_spaceLoop)
	{
		m_spaceLoop->Apply3D(m_listener, m_spaceEmitter);
	}

	if (m_retryDefault)
	{
		m_retryDefault = false;
		if (m_audEngine->Reset())
		{
			// Restart looping audio
			if (m_eyeLoop) {
				m_eyeLoop->Play(true);
			}
			if (m_spaceLoop) {
				m_spaceLoop->Play(true);
			}
		}
	}
	
	if (!m_audEngine->Update())
	{
		if (m_audEngine->IsCriticalError())
		{
			m_retryDefault = true;
		}
	}
  
	if (m_input.Quit())
	{
		ExitGame();
	}
}
#pragma endregion

#pragma region Frame Render
// Draws the scene.
void Game::Render()
{
	// Don't try to render anything before the first Update.
	if (m_timer.GetFrameCount() == 0)
	{
		return;
	}

	Clear();

	m_deviceResources->PIXBeginEvent(L"Render");
	auto context = m_deviceResources->GetD3DDeviceContext();
	auto renderTargetView = m_deviceResources->GetRenderTargetView();
	auto depthTargetView = m_deviceResources->GetDepthStencilView();

	//Set Rendering states. 
	context->OMSetBlendState(m_states->Opaque(), nullptr, 0xFFFFFFFF);
	context->OMSetDepthStencilState(m_states->DepthDefault(), 0);
	context->RSSetState(m_states->CullClockwise());


	//-------------------Planet 1--------------------------------

	m_world = SimpleMath::Matrix::Identity;

	// Quaternions for Rotation
	SimpleMath::Quaternion planetRotation = SimpleMath::Quaternion::CreateFromRotationMatrix(m_world);
	SimpleMath::Quaternion newRotation = SimpleMath::Quaternion::CreateFromYawPitchRoll((float)m_timer.GetTotalSeconds(), 0.f, 0.f);
	newRotation.Normalize(); planetRotation.Normalize();

	planetRotation *= newRotation;
	m_world = SimpleMath::Matrix::CreateFromQuaternion(planetRotation);

	SimpleMath::Matrix newScale = SimpleMath::Matrix::CreateScale(2.0f, 2.0f, 2.0f);

	// Translation
	SimpleMath::Matrix newPosition1 = SimpleMath::Matrix::CreateTranslation(3.f, 2.f, -3.f);
	m_world = m_world * newScale * newPosition1;

	m_BasicShaderPair.EnableShader(context);
	m_BasicShaderPair.SetShaderParameters(context, &m_world, &m_view, &m_projection, &m_Light, m_texture1.Get());
	m_BasicModel.Render(context);



	//-------------------Planet 2--------------------------------

	m_world = SimpleMath::Matrix::Identity;

	// Quaternions for Rotation
	SimpleMath::Quaternion planetRotation2 = SimpleMath::Quaternion::CreateFromRotationMatrix(m_world);
	SimpleMath::Quaternion planetRotation3 = SimpleMath::Quaternion::CreateFromAxisAngle(SimpleMath::Vector3(0.f, -1.f, -1.f), (float)m_timer.GetTotalSeconds());
	planetRotation3.Normalize(); planetRotation2.Normalize();

	SimpleMath::Matrix newRotation5 = SimpleMath::Matrix::CreateFromQuaternion(planetRotation2 * planetRotation3);

	// Translation
	SimpleMath::Matrix newPosition2 = SimpleMath::Matrix::CreateTranslation(3.f, 2.f, -3.f);
	SimpleMath::Matrix newPosition3 = SimpleMath::Matrix::CreateTranslation(-2.f, 0.f, 0.f);
	m_world = newPosition3 * newRotation5 * newPosition2;

	m_BasicShaderPair.EnableShader(context);
	m_BasicShaderPair.SetShaderParameters(context, &m_world, &m_view, &m_projection, &m_Light, m_texture3.Get());
	m_BasicModel.Render(context);

	
	//---------------------EyeBall Render------------------------------

	m_world = SimpleMath::Matrix::Identity;

	// Convert World Matrix into quaternion
	SimpleMath::Quaternion planetRotation4 = SimpleMath::Quaternion::CreateFromRotationMatrix(m_world);
	planetRotation4.Normalize();

	// Get a random target rotation to emulate "twitching"
	SimpleMath::Quaternion newRotation3 = m_Eye.GetTarget();

	// Use the saved orientation for the Eye to adjust world quaternion
	planetRotation4 *= m_Eye.GetOrientation();

	// Use counter to determine percentage for SLURPIE
	float angle = (float)(m_Eye.GetEyeballCounter() % 20) / 20.f;
	m_Eye.IterateEyeballCounter();

	// PERFORM SLURPIE
	SimpleMath::Quaternion newWorld = SimpleMath::Quaternion::Slerp(planetRotation4, newRotation3, angle);
	newWorld.Normalize();

	// Eye texture is backwards so necessary to invert the entire object after other rotations
	m_world = SimpleMath::Matrix::CreateFromQuaternion(newWorld) * SimpleMath::Matrix::CreateFromAxisAngle(SimpleMath::Vector3(0.f, 1.f, 0.f), XM_PI);

	// Render
	m_BasicShaderPair.EnableShader(context);
	m_BasicShaderPair.SetShaderParameters(context, &m_world, &m_view, &m_projection, &m_Light, m_texture2.Get());
	m_BasicModel.Render(context);

	//---------------------Particle System Render------------------------------
	
	// Snow System
	m_world = SimpleMath::Matrix::Identity * SimpleMath::Matrix::CreateFromAxisAngle(SimpleMath::Vector3(0.f, 1.f, 0.f), XM_PI);

	m_BasicShaderPair.EnableShader(context);
	m_BasicShaderPair.SetShaderParameters(context, &m_world, &m_view, &m_projection, &m_Light, m_SnowSystem.GetTexture().Get());
	m_SnowSystem.Render(context);


	// Fire System
	m_world = SimpleMath::Matrix::Identity * SimpleMath::Matrix::CreateFromAxisAngle(SimpleMath::Vector3(0.f, 1.f, 0.f), XM_PI);

	m_BasicShaderPair.EnableShader(context);
	m_BasicShaderPair.SetShaderParameters(context, &m_world, &m_view, &m_projection, &m_Light, m_FireSystem.GetTexture().Get());
	m_FireSystem.Render(context);

	// Black Hole System
	m_world = SimpleMath::Matrix::Identity * SimpleMath::Matrix::CreateFromAxisAngle(SimpleMath::Vector3(0.f, 1.f, 0.f), XM_PI);

	m_BasicShaderPair.EnableShader(context);
	m_BasicShaderPair.SetShaderParameters(context, &m_world, &m_view, &m_projection, &m_Light, m_SpaceSystem.GetTexture().Get());
	m_SpaceSystem.Render(context);

    // Show the new frame.
    m_deviceResources->Present();
}

// Helper method to clear the back buffers.
void Game::Clear()
{
    m_deviceResources->PIXBeginEvent(L"Clear");

    // Clear the views.
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto renderTarget = m_deviceResources->GetRenderTargetView();
    auto depthStencil = m_deviceResources->GetDepthStencilView();

	// Background Color Here
    context->ClearRenderTargetView(renderTarget, Colors::DarkViolet);
    context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    context->OMSetRenderTargets(1, &renderTarget, depthStencil);

    // Set the viewport.
    auto viewport = m_deviceResources->GetScreenViewport();
    context->RSSetViewports(1, &viewport);

    m_deviceResources->PIXEndEvent();
}

#pragma endregion

#pragma region Message Handlers
// Message handlers
void Game::OnActivated()
{
}

void Game::OnDeactivated()
{
}

void Game::OnSuspending()
{
	m_audEngine->Suspend();
}

void Game::OnResuming()
{
    m_timer.ResetElapsedTime();

	m_audEngine->Resume();
}

void Game::OnWindowMoved()
{
    auto r = m_deviceResources->GetOutputSize();
    m_deviceResources->WindowSizeChanged(r.right, r.bottom);
}

void Game::OnWindowSizeChanged(int width, int height)
{
    if (!m_deviceResources->WindowSizeChanged(width, height))
        return;

    CreateWindowSizeDependentResources();
}

// Properties
void Game::GetDefaultSize(int& width, int& height) const
{
    width = 800;
    height = 600;
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Game::CreateDeviceDependentResources()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto device = m_deviceResources->GetD3DDevice();

    m_states = std::make_unique<CommonStates>(device);
    m_fxFactory = std::make_unique<EffectFactory>(device);
    m_sprites = std::make_unique<SpriteBatch>(context);
    m_font = std::make_unique<SpriteFont>(device, L"SegoeUI_18.spritefont");
	m_batch = std::make_unique<PrimitiveBatch<VertexPositionColor>>(context);

	//setup our test model
	m_BasicModel.InitializeSphere(device);

	//load and set up our Vertex and Pixel Shaders
	m_BasicShaderPair.InitStandard(device, L"light_vs.cso", L"light_ps.cso");

	//load Textures
	CreateDDSTextureFromFile(device, L"black_hole.dds",		nullptr,	m_texture1.ReleaseAndGetAddressOf());
	CreateDDSTextureFromFile(device, L"eye.dds",			nullptr,	m_texture2.ReleaseAndGetAddressOf());
	CreateDDSTextureFromFile(device, L"seafloor.dds",		nullptr,	m_texture3.ReleaseAndGetAddressOf());

	//Setup Particle System
	m_FireSystem.Initialize(device,	 L"fire.dds", ParticleSystemClass::SystemType::Fire);
	m_SnowSystem.Initialize(device,  L"snow.dds", ParticleSystemClass::SystemType::Snow);
	m_SpaceSystem.Initialize(device, L"black_hole.dds", ParticleSystemClass::SystemType::Space);

}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    auto size = m_deviceResources->GetOutputSize();
    float aspectRatio = float(size.right) / float(size.bottom);
    float fovAngleY = 70.0f * XM_PI / 180.0f;

    // This is a simple example of change that can be made when the app is in
    // portrait or snapped view.
    if (aspectRatio < 1.0f)
    {
        fovAngleY *= 2.0f;
    }

    // This sample makes use of a right-handed coordinate system using row-major matrices.
    m_projection = Matrix::CreatePerspectiveFieldOfView(
        fovAngleY,
        aspectRatio,
        0.01f,
        100.0f
    );
}

void Game::OnDeviceLost()
{
    m_states.reset();
    m_fxFactory.reset();
    m_sprites.reset();
    m_font.reset();
	m_batch.reset();
	m_testmodel.reset();
    m_batchInputLayout.Reset();
}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion
