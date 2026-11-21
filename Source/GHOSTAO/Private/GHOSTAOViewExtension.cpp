// Powered by rly_ghost
#include "GHOSTAOViewExtension.h"

#include "GHOSTAORenderer.h"
#include "HAL/IConsoleManager.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "SceneView.h"

FGHOSTAOViewExtension::FGHOSTAOViewExtension(const FAutoRegister& AutoRegister)
	: FSceneViewExtensionBase(AutoRegister)
{
}

bool FGHOSTAOViewExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	static const auto CVarEnable = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.GHOSTAO.Enable"));
	return CVarEnable && CVarEnable->GetValueOnAnyThread() != 0;
}

void FGHOSTAOViewExtension::SubscribeToPostProcessingPass(EPostProcessingPass Pass, const FSceneView& InView,
	FPostProcessingPassDelegateArray& InOutPassCallbacks, bool bIsPassEnabled)
{
	if (Pass != EPostProcessingPass::BeforeDOF || !bIsPassEnabled)
	{
		return;
	}

	InOutPassCallbacks.Add(FPostProcessingPassDelegate::CreateRaw(this, &FGHOSTAOViewExtension::OnBeforeDOF));
}

FScreenPassTexture FGHOSTAOViewExtension::OnBeforeDOF(FRDGBuilder& GraphBuilder, const FSceneView& View,
	const FPostProcessMaterialInputs& Inputs)
{
	return GHOSTAO::AddPasses(GraphBuilder, View, Inputs);
}
