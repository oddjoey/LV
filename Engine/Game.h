#pragma once

// The seam between Lordi and game code. Lordi calls these hooks from its startup and main
// loop; a game overrides the ones it needs and is handed to Lordi::SetGame before Start.
class Game
{
public:
	virtual ~Game() = default;

	// Runs after the asset managers exist but before the renderer uploads anything to the GPU,
	// so models, textures, objects and cameras must be registered here.
	virtual void OnLoadAssets() {}
	// Runs once every engine system is up, just before the first frame.
	virtual void OnStart() {}
	virtual void OnUpdate(float deltaTime) {}
	// Runs every frame inside the ImGui frame.
	virtual void OnGUI() {}
	virtual void OnShutdown() {}
};
