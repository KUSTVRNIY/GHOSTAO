// Powered by rly_ghost
#include "GHOSTAORenderer.h"
#include "GHOSTAOApi.h"

#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "SceneRenderTargetParameters.h"
#include "SceneTexturesConfig.h"
#include "SceneView.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "CoreGlobals.h"
#include "HAL/IConsoleManager.h"
#include "Math/Matrix.h"

class FGHOSTAOPrepassCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FGHOSTAOPrepassCS);
	SHADER_USE_PARAMETER_STRUCT(FGHOSTAOPrepassCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, GhostAOSceneTextures)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, GhostAOPackedOut)
		SHADER_PARAMETER(FIntPoint, GhostAOPackedResolution)
		SHADER_PARAMETER(float, GhostAODepthScale)
		SHADER_PARAMETER(float, GhostAODepthOffset)
		SHADER_PARAMETER(uint32, GhostAORandomSeed)
		SHADER_PARAMETER(FMatrix44f, GhostAOClipToView)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADING_PATH_DEFERRED"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FGHOSTAOPrepassCS, "/Plugin/GHOSTAO/Private/GHOSTAOPrepass.usf", "MainPrepassCS", SF_Compute);

class FGHOSTAOMainPassCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FGHOSTAOMainPassCS);
	SHADER_USE_PARAMETER_STRUCT(FGHOSTAOMainPassCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<uint>, GhostAOPackedData)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, GhostAOOcclusion)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture3D<uint>, GhostAOOccluderLut)
		SHADER_PARAMETER(FVector4f, GhostAOLutParams)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, GhostAOBentNormalLut)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, GhostAOBentNormal)
		SHADER_PARAMETER(uint32, GhostAOPackedResolution)
		SHADER_PARAMETER(uint32, GhostAORandomSeed)
		SHADER_PARAMETER(float, GhostAONormalBias)
		SHADER_PARAMETER(float, GhostAORadius)
		SHADER_PARAMETER(FVector4f, GhostAOUnprojectParams)
		SHADER_PARAMETER(FVector4f, GhostAOProjectionParams)
		SHADER_PARAMETER(float, GhostAOMaxApparentCircleRadiusCoefficient)
		SHADER_PARAMETER(float, GhostAOMipBias)
		SHADER_PARAMETER(float, GhostAOInverseMaxSlopeCompensation)
		SHADER_PARAMETER(float, GhostAOBackfaceThreshold)
		SHADER_PARAMETER(float, GhostAOBackfaceBoost)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADING_PATH_DEFERRED"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FGHOSTAOMainPassCS, "/Plugin/GHOSTAO/Private/GHOSTAOMainPass.usf", "MainMainPassCS", SF_Compute);

class FGHOSTAOFilterPassCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FGHOSTAOFilterPassCS);
	SHADER_USE_PARAMETER_STRUCT(FGHOSTAOFilterPassCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<uint>, GhostAOPackedData)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float>, GhostAOOcclusionIn)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, GhostAOOcclusionOut)
		SHADER_PARAMETER(FIntPoint, GhostAOPackedResolution)
		SHADER_PARAMETER(float, GhostAODepthSensitivity)
		SHADER_PARAMETER(float, GhostAONormalSensitivity)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADING_PATH_DEFERRED"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FGHOSTAOFilterPassCS, "/Plugin/GHOSTAO/Private/GHOSTAOFilterPass.usf", "MainFilterPassCS", SF_Compute);

class FGHOSTAOTemporalCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FGHOSTAOTemporalCS);
	SHADER_USE_PARAMETER_STRUCT(FGHOSTAOTemporalCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<uint>, GhostAOPackedData)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float>, GhostAOAOIn)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float>, GhostAOAOHistory)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, GhostAOBentNormalIn)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, GhostAOBentNormalHistory)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, GhostAOAOOut)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, GhostAOBentNormalOut)
		SHADER_PARAMETER(FIntPoint, GhostAOResolution)
		SHADER_PARAMETER(FIntPoint, GhostAOHistoryResolution)
		SHADER_PARAMETER(FMatrix44f, GhostAOPrevViewProj)
		SHADER_PARAMETER(FMatrix44f, GhostAOViewToWorld)
		SHADER_PARAMETER(FVector4f, GhostAOUnprojectParams)
		SHADER_PARAMETER(float, GhostAOTemporalBlend)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADING_PATH_DEFERRED"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FGHOSTAOTemporalCS, "/Plugin/GHOSTAO/Private/GHOSTAOTemporal.usf", "MainTemporalCS", SF_Compute);

class FGHOSTAOCompositeCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FGHOSTAOCompositeCS);
	SHADER_USE_PARAMETER_STRUCT(FGHOSTAOCompositeCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, GhostAOSceneColor)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float>, GhostAOAO)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<uint>, GhostAOPackedData)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, GhostAOBentNormal)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, GhostAOSceneColorOut)
		SHADER_PARAMETER(FIntPoint, GhostAOResolution)
		SHADER_PARAMETER(FVector4f, GhostAOUnprojectParams)
		SHADER_PARAMETER(float, GhostAOIntensity)
		SHADER_PARAMETER(float, GhostAOSpecularOcclusion)
		SHADER_PARAMETER(uint32, GhostAOVisualizeBentNormals)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
IMPLEMENT_GLOBAL_SHADER(FGHOSTAOCompositeCS, "/Plugin/GHOSTAO/Private/GHOSTAOComposite.usf", "MainCompositeCS", SF_Compute);

static const FIntVector GGHOSTAOLutSize(24, 24, 16);

class FGHOSTAOLutCoverageCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FGHOSTAOLutCoverageCS);
	SHADER_USE_PARAMETER_STRUCT(FGHOSTAOLutCoverageCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<uint>, GhostAOLutOut)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
IMPLEMENT_GLOBAL_SHADER(FGHOSTAOLutCoverageCS, "/Plugin/GHOSTAO/Private/GHOSTAOLutGen.usf", "MainLutCoverageCS", SF_Compute);

class FGHOSTAOLutFillCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FGHOSTAOLutFillCS);
	SHADER_USE_PARAMETER_STRUCT(FGHOSTAOLutFillCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<uint>, GhostAOLutOut)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
IMPLEMENT_GLOBAL_SHADER(FGHOSTAOLutFillCS, "/Plugin/GHOSTAO/Private/GHOSTAOLutGen.usf", "MainLutFillCS", SF_Compute);

class FGHOSTAOBentNormalLutCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FGHOSTAOBentNormalLutCS);
	SHADER_USE_PARAMETER_STRUCT(FGHOSTAOBentNormalLutCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, GhostAOBentNormalLutOut)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
IMPLEMENT_GLOBAL_SHADER(FGHOSTAOBentNormalLutCS, "/Plugin/GHOSTAO/Private/GHOSTAOLutGen.usf", "MainBentNormalLutCS", SF_Compute);

static TAutoConsoleVariable<float> CVarGHOSTAORadius(
	TEXT("r.GHOSTAO.Radius"),
	100.0f,
	TEXT("GHOSTAO ambient occlusion radius in world units (cm)."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarGHOSTAOScreenRadiusRatio(
	TEXT("r.GHOSTAO.ScreenRadiusRatio"),
	0.2f,
	TEXT("GHOSTAO maximum screen space radius as a fraction of screen height."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarGHOSTAOTemporal(
	TEXT("r.GHOSTAO.Temporal"),
	1,
	TEXT("Randomize GHOSTAO sampling every frame (1) or keep it stable for screenshots (0)."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarGHOSTAOTemporalBlend(
	TEXT("r.GHOSTAO.TemporalBlend"),
	0.1f,
	TEXT("Weight of the new frame in the temporal AO accumulation (0.01..1, 1 disables history)."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarGHOSTAOIntensity(
	TEXT("r.GHOSTAO.Intensity"),
	1.0f,
	TEXT("Strength of the ambient occlusion applied to the scene color (0..1)."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarGHOSTAOSpecularOcclusion(
	TEXT("r.GHOSTAO.SpecularOcclusion"),
	0.35f,
	TEXT("Strength of the bent-normal based specular occlusion term (0 disables it)."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarGHOSTAOVisualizeBentNormals(
	TEXT("r.GHOSTAO.VisualizeBentNormals"),
	0,
	TEXT("Visualize the view-space bent normal instead of the final color (0: off, 1: on)."),
	ECVF_RenderThreadSafe);

namespace GHOSTAO
{
	static FMatrix44f ToMatrix44f(const FMatrix& M)
	{
		FMatrix44f Out;
		for (int32 Row = 0; Row < 4; ++Row)
		{
			for (int32 Col = 0; Col < 4; ++Col)
			{
				Out.M[Row][Col] = (float)M.M[Row][Col];
			}
		}
		return Out;
	}

	struct FHistoryEntry
	{
		TRefCountPtr<IPooledRenderTarget> Texture;
		TRefCountPtr<IPooledRenderTarget> BentNormalTexture;
		FIntPoint Size = FIntPoint::ZeroValue;
		FMatrix44f PrevViewProj = FMatrix44f::Identity;
		bool bHasHistory = false;
	};

	static TMap<const void*, FHistoryEntry> GGHOSTAOHistory;

	FScreenPassTexture AddPasses(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs)
	{
		const FScreenPassTextureSlice& ColorSlice = Inputs.Textures[0];
		if (!ColorSlice.IsValid() || ColorSlice.TextureSRV == nullptr)
		{
			return Inputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder);
		}

		FRDGTextureRef SceneColorTexture = ColorSlice.TextureSRV->Desc.Texture;
		const FIntPoint Extent = SceneColorTexture->Desc.Extent;
		const FIntRect ViewRect = ColorSlice.ViewRect;

		if (Extent.X <= 0 || Extent.Y <= 0)
		{
			return Inputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder);
		}

		static bool bLoggedFirstDispatch = false;
		if (!bLoggedFirstDispatch)
		{
			bLoggedFirstDispatch = true;
			UE_LOG(LogTemp, Warning, TEXT("[GHOSTAO] First AO pass dispatch: %dx%d"), Extent.X, Extent.Y);
		}

		const FMatrix Projection = View.ViewMatrices.GetProjectionMatrix();
		const FMatrix InvProjection = View.ViewMatrices.GetInvProjectionMatrix();

		const float TanHalfFovX = (float)InvProjection.M[0][0];
		const float TanHalfFovY = (float)InvProjection.M[1][1];

		uint32 RandomSeed = 0u;
		if (CVarGHOSTAOTemporal.GetValueOnRenderThread() != 0)
		{
			RandomSeed = (uint32)(GFrameCounterRenderThread * 747796405u + 2891336453u);
		}

		const FIntPoint PackedResolution(Extent.X, Extent.Y);
		const uint32 PackedResolutionPacked = (uint32)Extent.X | ((uint32)Extent.Y << 16u);

		FRDGTextureDesc PackedDesc = FRDGTextureDesc::Create2D(
			Extent, PF_R32_UINT, FClearValueBinding::None,
			TexCreate_ShaderResource | TexCreate_UAV, 1);
		FRDGTextureRef PackedTexture = GraphBuilder.CreateTexture(PackedDesc, TEXT("GHOSTAO.Packed"));

		FRDGTextureDesc AODesc = FRDGTextureDesc::Create2D(
			Extent, PF_R32_FLOAT, FClearValueBinding::None,
			TexCreate_ShaderResource | TexCreate_UAV, 1);
		FRDGTextureRef AORawTexture = GraphBuilder.CreateTexture(AODesc, TEXT("GHOSTAO.OcclusionRaw"));
		FRDGTextureRef AOFilteredTexture = GraphBuilder.CreateTexture(AODesc, TEXT("GHOSTAO.OcclusionFiltered"));
		FRDGTextureRef AOFiltered2Texture = GraphBuilder.CreateTexture(AODesc, TEXT("GHOSTAO.OcclusionFiltered2"));

		TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextureUniformBuffer =
			CreateSceneTextureUniformBuffer(GraphBuilder, View, ESceneTextureSetupMode::SceneDepth);

		{
			TShaderMapRef<FGHOSTAOPrepassCS> Shader(GetGlobalShaderMap(View.GetFeatureLevel()));
			FGHOSTAOPrepassCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FGHOSTAOPrepassCS::FParameters>();
			PassParameters->GhostAOSceneTextures = SceneTextureUniformBuffer;
			PassParameters->GhostAOPackedOut = GraphBuilder.CreateUAV(PackedTexture);
			PassParameters->GhostAOPackedResolution = PackedResolution;
			PassParameters->GhostAODepthScale = 1.0f / FMath::Max((float)View.NearClippingDistance, 1e-3f);
			PassParameters->GhostAODepthOffset = 0.0f;
			PassParameters->GhostAORandomSeed = RandomSeed;
			PassParameters->GhostAOClipToView = ToMatrix44f(InvProjection);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("GHOSTAO Prepass"),
				ERDGPassFlags::Compute,
				Shader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(Extent, 8));
		}

		FRDGTextureDesc LutDesc = FRDGTextureDesc::Create3D(
			GGHOSTAOLutSize, PF_R32_UINT, FClearValueBinding::None,
			TexCreate_ShaderResource | TexCreate_UAV, 1);
		FRDGTextureRef LutTexture = GraphBuilder.CreateTexture(LutDesc, TEXT("GHOSTAO.OccluderLut"));

		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(LutTexture), 0u);

		{
			TShaderMapRef<FGHOSTAOLutCoverageCS> Shader(GetGlobalShaderMap(View.GetFeatureLevel()));
			FGHOSTAOLutCoverageCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FGHOSTAOLutCoverageCS::FParameters>();
			PassParameters->GhostAOLutOut = GraphBuilder.CreateUAV(LutTexture);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("GHOSTAO LUT Coverage"),
				ERDGPassFlags::Compute,
				Shader,
				PassParameters,
				FIntVector(
					FMath::DivideAndRoundUp(GGHOSTAOLutSize.X, 4),
					FMath::DivideAndRoundUp(GGHOSTAOLutSize.Y, 4),
					FMath::DivideAndRoundUp(GGHOSTAOLutSize.Z, 4)));
		}

		{
			TShaderMapRef<FGHOSTAOLutFillCS> Shader(GetGlobalShaderMap(View.GetFeatureLevel()));
			FGHOSTAOLutFillCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FGHOSTAOLutFillCS::FParameters>();
			PassParameters->GhostAOLutOut = GraphBuilder.CreateUAV(LutTexture);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("GHOSTAO LUT Fill"),
				ERDGPassFlags::Compute,
				Shader,
				PassParameters,
				FIntVector(1, 1, 1));
		}

		FRDGTextureDesc BentNormalLutDesc = FRDGTextureDesc::Create2D(
			FIntPoint(256, 256), PF_FloatRGBA, FClearValueBinding::None,
			TexCreate_ShaderResource | TexCreate_UAV, 1);
		FRDGTextureRef BentNormalLutTexture = GraphBuilder.CreateTexture(BentNormalLutDesc, TEXT("GHOSTAO.BentNormalLut"));

		{
			TShaderMapRef<FGHOSTAOBentNormalLutCS> Shader(GetGlobalShaderMap(View.GetFeatureLevel()));
			FGHOSTAOBentNormalLutCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FGHOSTAOBentNormalLutCS::FParameters>();
			PassParameters->GhostAOBentNormalLutOut = GraphBuilder.CreateUAV(BentNormalLutTexture);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("GHOSTAO BentNormal LUT"),
				ERDGPassFlags::Compute,
				Shader,
				PassParameters,
				FIntVector(16, 16, 1));
		}

		FRDGTextureDesc BentNormalDesc = FRDGTextureDesc::Create2D(
			Extent, PF_FloatRGBA, FClearValueBinding::None,
			TexCreate_ShaderResource | TexCreate_UAV, 1);
		FRDGTextureRef BentNormalTexture = GraphBuilder.CreateTexture(BentNormalDesc, TEXT("GHOSTAO.BentNormal"));

		{
			float MaxApparentCircleRadius = TanHalfFovY * CVarGHOSTAOScreenRadiusRatio.GetValueOnRenderThread() * 2.0f;

			TShaderMapRef<FGHOSTAOMainPassCS> Shader(GetGlobalShaderMap(View.GetFeatureLevel()));
			FGHOSTAOMainPassCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FGHOSTAOMainPassCS::FParameters>();
			PassParameters->GhostAOPackedData = GraphBuilder.CreateSRV(PackedTexture);
			PassParameters->GhostAOOcclusion = GraphBuilder.CreateUAV(AORawTexture);
			PassParameters->GhostAOOccluderLut = GraphBuilder.CreateSRV(LutTexture);
			PassParameters->GhostAOLutParams = FVector4f(
				(float)GGHOSTAOLutSize.X, (float)GGHOSTAOLutSize.Y, (float)GGHOSTAOLutSize.Z, 0.0f);
			PassParameters->GhostAOBentNormalLut = GraphBuilder.CreateSRV(BentNormalLutTexture);
			PassParameters->GhostAOBentNormal = GraphBuilder.CreateUAV(BentNormalTexture);
			PassParameters->GhostAOPackedResolution = PackedResolutionPacked;
			PassParameters->GhostAORandomSeed = RandomSeed;
			PassParameters->GhostAONormalBias = TanHalfFovX / float(Extent.X) * FMath::Sqrt(2.0f);
			PassParameters->GhostAORadius = CVarGHOSTAORadius.GetValueOnRenderThread();
			PassParameters->GhostAOUnprojectParams = FVector4f(2.0f * TanHalfFovX, -2.0f * TanHalfFovY, -TanHalfFovX, TanHalfFovY);
			PassParameters->GhostAOProjectionParams = FVector4f(
				0.5f * (float)Projection.M[0][0],
				-0.5f * (float)Projection.M[1][1],
				-0.5f,
				-0.5f);
			PassParameters->GhostAOMaxApparentCircleRadiusCoefficient =
				MaxApparentCircleRadius / FMath::Sqrt(MaxApparentCircleRadius * MaxApparentCircleRadius + 1.0f);
			PassParameters->GhostAOMipBias = 0.0f;
			PassParameters->GhostAOInverseMaxSlopeCompensation = 0.25f;
			PassParameters->GhostAOBackfaceThreshold = 0.5f;
			PassParameters->GhostAOBackfaceBoost = 0.0f;

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("GHOSTAO MainPass"),
				ERDGPassFlags::Compute,
				Shader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(Extent, 16));
		}

		{
			TShaderMapRef<FGHOSTAOFilterPassCS> Shader(GetGlobalShaderMap(View.GetFeatureLevel()));
			FGHOSTAOFilterPassCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FGHOSTAOFilterPassCS::FParameters>();
			PassParameters->GhostAOPackedData = GraphBuilder.CreateSRV(PackedTexture);
			PassParameters->GhostAOOcclusionIn = GraphBuilder.CreateSRV(AORawTexture);
			PassParameters->GhostAOOcclusionOut = GraphBuilder.CreateUAV(AOFilteredTexture);
			PassParameters->GhostAOPackedResolution = PackedResolution;
			PassParameters->GhostAODepthSensitivity = 512.0f;
			PassParameters->GhostAONormalSensitivity = 128.0f;

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("GHOSTAO FilterPass"),
				ERDGPassFlags::Compute,
				Shader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(FIntPoint(FMath::DivideAndRoundUp(Extent.X, 16), FMath::DivideAndRoundUp(Extent.Y, 16)), 1));
		}

		{
			TShaderMapRef<FGHOSTAOFilterPassCS> Shader(GetGlobalShaderMap(View.GetFeatureLevel()));
			FGHOSTAOFilterPassCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FGHOSTAOFilterPassCS::FParameters>();
			PassParameters->GhostAOPackedData = GraphBuilder.CreateSRV(PackedTexture);
			PassParameters->GhostAOOcclusionIn = GraphBuilder.CreateSRV(AOFilteredTexture);
			PassParameters->GhostAOOcclusionOut = GraphBuilder.CreateUAV(AOFiltered2Texture);
			PassParameters->GhostAOPackedResolution = PackedResolution;
			PassParameters->GhostAODepthSensitivity = 512.0f;
			PassParameters->GhostAONormalSensitivity = 128.0f;

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("GHOSTAO FilterPass2"),
				ERDGPassFlags::Compute,
				Shader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(FIntPoint(FMath::DivideAndRoundUp(Extent.X, 16), FMath::DivideAndRoundUp(Extent.Y, 16)), 1));
		}

		FRDGTextureDesc AOAccumDesc = FRDGTextureDesc::Create2D(
			Extent, PF_R32_FLOAT, FClearValueBinding::None,
			TexCreate_ShaderResource | TexCreate_UAV, 1);
		FRDGTextureRef AOAccumTexture = GraphBuilder.CreateTexture(AOAccumDesc, TEXT("GHOSTAO.OcclusionAccumulated"));

		FRDGTextureDesc BentNormalAccumDesc = FRDGTextureDesc::Create2D(
			Extent, PF_FloatRGBA, FClearValueBinding::None,
			TexCreate_ShaderResource | TexCreate_UAV, 1);
		FRDGTextureRef BentNormalAccumTexture = GraphBuilder.CreateTexture(BentNormalAccumDesc, TEXT("GHOSTAO.BentNormalAccumulated"));

		float TemporalBlend = FMath::Clamp(CVarGHOSTAOTemporalBlend.GetValueOnRenderThread(), 0.01f, 1.0f);
		uint64 ViewKeyBits = (View.Family != nullptr && View.Family->Scene != nullptr)
			? (uint64)(UPTRINT)View.Family->Scene
			: 0ull;
		const void* ViewKey = (const void*)ViewKeyBits;
		FHistoryEntry* HistoryEntry = ViewKey != nullptr ? GGHOSTAOHistory.Find(ViewKey) : nullptr;
		const bool bHistoryValid = HistoryEntry != nullptr
			&& HistoryEntry->bHasHistory
			&& HistoryEntry->Texture.IsValid();

		FRDGTextureRef HistoryTexture = bHistoryValid
			? GraphBuilder.RegisterExternalTexture(HistoryEntry->Texture, TEXT("GHOSTAO.HistoryAO"))
			: AOFilteredTexture;

		FRDGTextureRef BentNormalHistoryTexture = (bHistoryValid && HistoryEntry->BentNormalTexture.IsValid())
			? GraphBuilder.RegisterExternalTexture(HistoryEntry->BentNormalTexture, TEXT("GHOSTAO.HistoryBentNormal"))
			: BentNormalTexture;

		const FMatrix44f PrevViewProj = bHistoryValid
			? HistoryEntry->PrevViewProj
			: ToMatrix44f(View.ViewMatrices.GetViewProjectionMatrix());

		if (!bHistoryValid)
		{
			TemporalBlend = 1.0f;
		}
		else
		{
			static bool GGHOSTAOLoggedHistoryActive = false;
			if (!GGHOSTAOLoggedHistoryActive)
			{
				GGHOSTAOLoggedHistoryActive = true;
				UE_LOG(LogTemp, Log, TEXT("[GHOSTAO] Temporal history active."));
			}
		}

		{
			TShaderMapRef<FGHOSTAOTemporalCS> Shader(GetGlobalShaderMap(View.GetFeatureLevel()));
			FGHOSTAOTemporalCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FGHOSTAOTemporalCS::FParameters>();
			PassParameters->GhostAOPackedData = GraphBuilder.CreateSRV(PackedTexture);
			PassParameters->GhostAOAOIn = GraphBuilder.CreateSRV(AOFiltered2Texture);
			PassParameters->GhostAOAOHistory = GraphBuilder.CreateSRV(HistoryTexture);
			PassParameters->GhostAOBentNormalIn = GraphBuilder.CreateSRV(BentNormalTexture);
			PassParameters->GhostAOBentNormalHistory = GraphBuilder.CreateSRV(BentNormalHistoryTexture);
			PassParameters->GhostAOAOOut = GraphBuilder.CreateUAV(AOAccumTexture);
			PassParameters->GhostAOBentNormalOut = GraphBuilder.CreateUAV(BentNormalAccumTexture);
			PassParameters->GhostAOResolution = Extent;
			PassParameters->GhostAOHistoryResolution = (bHistoryValid && HistoryEntry != nullptr)
				? HistoryEntry->Size
				: Extent;
			PassParameters->GhostAOPrevViewProj = PrevViewProj;
			PassParameters->GhostAOViewToWorld = ToMatrix44f(View.ViewMatrices.GetInvViewMatrix());
			PassParameters->GhostAOUnprojectParams = FVector4f(2.0f * TanHalfFovX, -2.0f * TanHalfFovY, -TanHalfFovX, TanHalfFovY);
			PassParameters->GhostAOTemporalBlend = TemporalBlend;

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("GHOSTAO Temporal"),
				ERDGPassFlags::Compute,
				Shader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(Extent, 8));
		}

		if (ViewKey != nullptr)
		{
			if (GGHOSTAOHistory.Num() > 256)
			{
				GGHOSTAOHistory.Empty();
			}

			FHistoryEntry& Entry = GGHOSTAOHistory.FindOrAdd(ViewKey);
			Entry.Size = Extent;
			Entry.bHasHistory = true;
			Entry.PrevViewProj = ToMatrix44f(View.ViewMatrices.GetViewProjectionMatrix());
			GraphBuilder.QueueTextureExtraction(AOAccumTexture, &Entry.Texture);
			GraphBuilder.QueueTextureExtraction(BentNormalAccumTexture, &Entry.BentNormalTexture);
		}

		FRDGTextureDesc OutputDesc = FRDGTextureDesc::Create2D(
			Extent, PF_FloatRGBA, FClearValueBinding::None,
			TexCreate_ShaderResource | TexCreate_UAV, 1);
		FRDGTextureRef OutputTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("GHOSTAO.SceneColor"));

		{
			TShaderMapRef<FGHOSTAOCompositeCS> Shader(GetGlobalShaderMap(View.GetFeatureLevel()));
			FGHOSTAOCompositeCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FGHOSTAOCompositeCS::FParameters>();
			PassParameters->GhostAOSceneColor = ColorSlice.TextureSRV;
			PassParameters->GhostAOAO = GraphBuilder.CreateSRV(AOAccumTexture);
			PassParameters->GhostAOPackedData = GraphBuilder.CreateSRV(PackedTexture);
			PassParameters->GhostAOBentNormal = GraphBuilder.CreateSRV(BentNormalAccumTexture);
			PassParameters->GhostAOSceneColorOut = GraphBuilder.CreateUAV(OutputTexture);
			PassParameters->GhostAOResolution = Extent;
			PassParameters->GhostAOUnprojectParams = FVector4f(2.0f * TanHalfFovX, -2.0f * TanHalfFovY, -TanHalfFovX, TanHalfFovY);
			PassParameters->GhostAOIntensity = FMath::Clamp(CVarGHOSTAOIntensity.GetValueOnRenderThread(), 0.0f, 1.0f);
			PassParameters->GhostAOSpecularOcclusion = FMath::Clamp(CVarGHOSTAOSpecularOcclusion.GetValueOnRenderThread(), 0.0f, 1.0f);
			PassParameters->GhostAOVisualizeBentNormals = (uint32)FMath::Max(CVarGHOSTAOVisualizeBentNormals.GetValueOnRenderThread(), 0);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("GHOSTAO Composite"),
				ERDGPassFlags::Compute,
				Shader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(Extent, 8));
		}

		return FScreenPassTexture(OutputTexture, ViewRect);
	}
}

TRefCountPtr<IPooledRenderTarget> GHOSTAO::GetBentNormalTexture(const FSceneViewStateInterface* ViewState)
{
	if (ViewState == nullptr)
	{
		return TRefCountPtr<IPooledRenderTarget>();
	}

	const GHOSTAO::FHistoryEntry* Entry = GHOSTAO::GGHOSTAOHistory.Find(reinterpret_cast<const void*>(ViewState));
	return Entry != nullptr ? Entry->BentNormalTexture : TRefCountPtr<IPooledRenderTarget>();
}
