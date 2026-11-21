// Powered by rly_ghost
#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"

class FSceneViewStateInterface;

namespace GHOSTAO
{

	TRefCountPtr<IPooledRenderTarget> GetBentNormalTexture(const FSceneViewStateInterface* ViewState);
}
