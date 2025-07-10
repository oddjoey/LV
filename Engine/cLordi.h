#pragma once

#include "cGraphics.h"

class cLordi
{
	static std::shared_ptr<cLordi> _instance;
public:
	static std::shared_ptr<cLordi> Get();
	std::shared_ptr<cGraphics> GetGraphics() const;

	cLordi();
	~cLordi();
private:
	// Initialize engine systems
	bool InitializeSystems();

	void MainLoop();
};

