#include "DerClou3.h"

#include "DCWatchdog.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogDerClou);

class FDerClou3Module final : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		DCWatchdog::Start();
	}

	virtual void ShutdownModule() override
	{
		DCWatchdog::Stop();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FDerClou3Module, DerClou3, "DerClou3");
