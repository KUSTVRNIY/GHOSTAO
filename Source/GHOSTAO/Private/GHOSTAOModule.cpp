// Powered by rly_ghost
#include "GHOSTAOModule.h"

#include "GHOSTAOViewExtension.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Paths.h"
#include "SceneViewExtension.h"
#include "ShaderCore.h"

IMPLEMENT_MODULE(FGHOSTAOModule, GHOSTAO)

static TAutoConsoleVariable<int32> CVarGHOSTAOEnable(
	TEXT("r.GHOSTAO.Enable"),
	0,
	TEXT("Enable the GHOSTAO ambient occlusion passes (0: off, 1: on)."),
	ECVF_RenderThreadSafe);

static TSharedPtr<FGHOSTAOViewExtension, ESPMode::ThreadSafe> GGHOSTAOViewExtension;
static FDelegateHandle GGHOSTAOEngineInitHandle;

static void CreateGHOSTAOViewExtension()
{
	if (!GGHOSTAOViewExtension.IsValid() && GEngine != nullptr)
	{
		GGHOSTAOViewExtension = FSceneViewExtensions::NewExtension<FGHOSTAOViewExtension>();
		UE_LOG(LogTemp, Log, TEXT("[GHOSTAO] SceneViewExtension registered."));
	}
}

void FGHOSTAOModule::StartupModule()
{
	TSharedPtr<IPlugin, ESPMode::ThreadSafe> Plugin = IPluginManager::Get().FindPlugin(TEXT("GHOSTAO"));
	if (Plugin.IsValid())
	{
		const FString PluginShaderDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders"));
		AddShaderSourceDirectoryMapping(TEXT("/Plugin/GHOSTAO"), PluginShaderDir);
		UE_LOG(LogTemp, Log, TEXT("[GHOSTAO] Shader directory mapped: /Plugin/GHOSTAO -> %s"), *PluginShaderDir);
	}

	if (GEngine != nullptr)
	{
		CreateGHOSTAOViewExtension();
	}
	else
	{
		GGHOSTAOEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddStatic(&CreateGHOSTAOViewExtension);
		UE_LOG(LogTemp, Log, TEXT("[GHOSTAO] Waiting for engine init before registering SceneViewExtension."));
	}
}

void FGHOSTAOModule::ShutdownModule()
{
	if (GGHOSTAOEngineInitHandle.IsValid())
	{
		FCoreDelegates::OnPostEngineInit.Remove(GGHOSTAOEngineInitHandle);
		GGHOSTAOEngineInitHandle.Reset();
	}

	GGHOSTAOViewExtension.Reset();
	UE_LOG(LogTemp, Log, TEXT("[GHOSTAO] Plugin module unloaded."));
}
