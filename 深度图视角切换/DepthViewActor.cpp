#include "DepthViewActor.h"

#include "Engine/PostProcessVolume.h"
#include "Engine/Scene.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialExpressionSceneTexture.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "MaterialDomain.h"
#include "GameFramework/PlayerController.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"

#if WITH_EDITORONLY_DATA
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#endif

ADepthViewActor::ADepthViewActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

void ADepthViewActor::BeginPlay()
{
    Super::BeginPlay();
    BuildDepthMaterial();
    ApplyBlendWeight();
}

void ADepthViewActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // 延迟绑定 F8（PlayerController 可能比本 Actor 晚生成）
    if (!bInputBound)
    {
        APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
        if (PC && PC->InputComponent)
        {
            PC->InputComponent->BindKey(EKeys::F8, IE_Pressed, this, &ADepthViewActor::ToggleDepthView);
            bInputBound = true;
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan,
                    TEXT("DepthView ready: 按 F8 切换深度图 / F8 to toggle depth view"));
            }
        }
    }
}

#if WITH_EDITORONLY_DATA
// 搭建深度后处理材质的表达式图：SceneDepth(R) -> /MaxDepth -> 1-x -> 灰度输出
static void BuildDepthMaterialGraph(UMaterial* Mat, float MaxDepthDefault)
{
    Mat->MaterialDomain = MD_PostProcess;

    auto Add = [&](UMaterialExpression* E) { Mat->GetExpressionCollection().AddExpression(E); };

    UMaterialExpressionSceneTexture* SceneTex = NewObject<UMaterialExpressionSceneTexture>(Mat);
    SceneTex->SceneTextureId = PPI_SceneDepth;   // 线性深度，R 通道
    Add(SceneTex);

    UMaterialExpressionComponentMask* Mask = NewObject<UMaterialExpressionComponentMask>(Mat);
    Mask->R = true; Mask->G = false; Mask->B = false; Mask->A = false;
    Mask->Input.Expression = SceneTex;
    Mask->Input.OutputIndex = 0;
    Add(Mask);

    UMaterialExpressionScalarParameter* MaxDepth = NewObject<UMaterialExpressionScalarParameter>(Mat);
    MaxDepth->ParameterName = TEXT("MaxDepth");
    MaxDepth->DefaultValue = MaxDepthDefault;
    Add(MaxDepth);

    UMaterialExpressionDivide* Div = NewObject<UMaterialExpressionDivide>(Mat);
    Div->A.Expression = Mask;     Div->A.OutputIndex = 0;
    Div->B.Expression = MaxDepth; Div->B.OutputIndex = 0;
    Add(Div);

    UMaterialExpressionOneMinus* OneMinus = NewObject<UMaterialExpressionOneMinus>(Mat);
    OneMinus->Input.Expression = Div;
    OneMinus->Input.OutputIndex = 0;
    Add(OneMinus);

    Mat->GetEditorOnlyData()->EmissiveColor.Expression = OneMinus;
    Mat->GetEditorOnlyData()->EmissiveColor.OutputIndex = 0;

    Mat->PostEditChange();   // 触发 shader 编译
}
#endif

void ADepthViewActor::BuildDepthMaterial()
{
    if (DepthMID) return;

    UMaterialInterface* BaseMat = nullptr;

    // 1) 优先使用指定的材质资源（可随包烘焙，打包版本必须走这条）
    if (!DepthMaterial.IsNull())
    {
        BaseMat = DepthMaterial.LoadSynchronous();
        if (!BaseMat)
        {
            UE_LOG(LogTemp, Warning, TEXT("DepthView: DepthMaterial 资源加载失败: %s"), *DepthMaterial.ToString());
        }
    }

    // 2) 未指定资源时，仅在编辑器/PIE 里运行时临时生成（打包版无 shader 编译器，不生效）
#if WITH_EDITORONLY_DATA
    if (!BaseMat)
    {
        UMaterial* Mat = NewObject<UMaterial>(GetTransientPackage(), TEXT("M_DepthViewRuntime"));
        BuildDepthMaterialGraph(Mat, DepthMaxDistanceCm);
        BaseMat = Mat;
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Yellow,
                TEXT("DepthView: 运行时临时生成材质（仅 PIE 生效）。打包前请点 CreateAndSaveDepthMaterial 烘焙资源。"));
        }
    }
#endif

    if (!BaseMat)
    {
        UE_LOG(LogTemp, Error, TEXT("DepthView: 没有可用材质。请在编辑器中点 CreateAndSaveDepthMaterial，再把生成的 M_DepthView 赋给 DepthMaterial。"));
        return;
    }

    // MID：运行时改 MaxDepth 参数
    DepthMID = UMaterialInstanceDynamic::Create(BaseMat, this);

    // 全局 Unbound PostProcessVolume 挂载（weight=0 默认关）
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    DepthPPVolume = GetWorld()->SpawnActor<APostProcessVolume>(Params);
    if (DepthPPVolume)
    {
        DepthPPVolume->bUnbound = true;
        FWeightedBlendable Blend;
        Blend.Object = DepthMID;
        Blend.Weight = 0.0f;
        DepthPPVolume->Settings.WeightedBlendables.Array.Add(Blend);
    }
}

#if WITH_EDITORONLY_DATA
void ADepthViewActor::CreateAndSaveDepthMaterial()
{
    const FString PackageName = TEXT("/Game/DepthView/M_DepthView");
    const FString AssetName = TEXT("M_DepthView");

    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        UE_LOG(LogTemp, Error, TEXT("DepthView: CreatePackage 失败"));
        return;
    }

    // 若资源已存在，先清空旧图再覆盖
    UMaterial* Mat = FindObject<UMaterial>(Package, *AssetName);
    if (!Mat)
    {
        Mat = NewObject<UMaterial>(Package, *AssetName, RF_Public | RF_Standalone);
    }
    else
    {
        Mat->GetExpressionCollection().Empty();
    }

    BuildDepthMaterialGraph(Mat, DepthMaxDistanceCm);

    Package->MarkPackageDirty();
    const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
    const bool bSaved = UPackage::SavePackage(Package, Mat, RF_Public | RF_Standalone, *Filename);
    if (bSaved)
    {
        // 自动把生成的资源指给 DepthMaterial，运行时即可加载
        DepthMaterial = Mat;
        UE_LOG(LogTemp, Log, TEXT("DepthView: 已保存材质资源 %s"), *Filename);
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
                FString::Printf(TEXT("DepthView: 已烘焙并保存 %s，已自动赋给 DepthMaterial"), *Filename));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("DepthView: SavePackage 失败: %s"), *Filename);
    }
}
#endif

void ADepthViewActor::ApplyBlendWeight()
{
    if (!DepthPPVolume || !DepthMID) return;
    const float W = bDepthViewEnabled ? 1.0f : 0.0f;
    for (FWeightedBlendable& B : DepthPPVolume->Settings.WeightedBlendables.Array)
    {
        if (B.Object == DepthMID)
        {
            B.Weight = W;
            return;
        }
    }
}

void ADepthViewActor::ToggleDepthView()
{
    bDepthViewEnabled = !bDepthViewEnabled;
    ApplyBlendWeight();
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan,
            bDepthViewEnabled ? TEXT("DepthView: ON (深度图)") : TEXT("DepthView: OFF (正常画面)"));
    }
}

void ADepthViewActor::SetDepthViewEnabled(bool bEnable)
{
    bDepthViewEnabled = bEnable;
    ApplyBlendWeight();
}

void ADepthViewActor::SetDepthMaxDistance(float MaxDistanceCm)
{
    DepthMaxDistanceCm = MaxDistanceCm;
    if (DepthMID)
    {
        DepthMID->SetScalarParameterValue(TEXT("MaxDepth"), DepthMaxDistanceCm);
    }
}
