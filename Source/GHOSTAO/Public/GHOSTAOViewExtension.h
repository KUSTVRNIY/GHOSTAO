// Powered by rly_ghost
#pragma once

#include "CoreMinimal.h"
#include "SceneViewExtension.h"
#include "SceneViewExtensionContext.h"
#include "RendererInterface.h"
#include "PostProcess/PostProcessMaterialInputs.h"

class FGHOSTAOViewExtension : public FSceneViewExtensionBase
{
public:
	explicit FGHOSTAOViewExtension(const FAutoRegister& AutoRegister);

	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;
	virtual void SubscribeToPostProcessingPass(EPostProcessingPass Pass, const FSceneView& InView, FPostProcessingPassDelegateArray& InOutPassCallbacks, bool bIsPassEnabled) override;

private:
	FScreenPassTexture OnBeforeDOF(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs);
};
