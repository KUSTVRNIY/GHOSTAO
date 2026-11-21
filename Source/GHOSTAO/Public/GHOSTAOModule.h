// Powered by rly_ghost
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FGHOSTAOModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
