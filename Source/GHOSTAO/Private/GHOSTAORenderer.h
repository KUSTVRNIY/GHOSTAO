// Powered by rly_ghost
#pragma once

#include "CoreMinimal.h"
#include "ScreenPass.h"

class FRDGBuilder;
class FSceneView;
struct FPostProcessMaterialInputs;

namespace GHOSTAO
{

	FScreenPassTexture AddPasses(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs);
}
