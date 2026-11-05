// Fill out your copyright notice in the Description of Project Settings.

#include "SplinePointMarkerBase.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"

namespace
{
	// BasicShapes 的 Sphere / Cylinder 默认网格尺寸都是 100 单位
	constexpr float DESIGN_SPHERE_DIAMETER = 100.0f;
	constexpr float DESIGN_GIZMO_LENGTH    = 135.0f;   // 圆柱 scale Z=1.35 → 长 135
	constexpr float GIZMO_HALF_OFFSET      = 67.5f;    // = DESIGN_GIZMO_LENGTH / 2
}

ASplinePointMarkerBase::ASplinePointMarkerBase()
{
	PrimaryActorTick.bCanEverTick = true;

	// ========== 球体（根组件） ==========
	SphereMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SphereMesh"));
	RootComponent = SphereMesh;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMeshAsset.Succeeded()) SphereMesh->SetStaticMesh(SphereMeshAsset.Object);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	SphereMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SphereMesh->SetCollisionObjectType(ECC_WorldDynamic);
	SphereMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	SphereMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	SphereMesh->SetCastShadow(false);

	// ========== Gizmo 根 ==========
	GizmoRoot = CreateDefaultSubobject<USceneComponent>(TEXT("GizmoRoot"));
	GizmoRoot->SetupAttachment(SphereMesh);
	GizmoRoot->SetVisibility(false);

	// 三根圆柱手柄（BasicShapes/Cylinder 默认直径 100、高 100）
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMeshAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	GizmoX = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GizmoX"));
	GizmoX->SetupAttachment(GizmoRoot);
	GizmoX->SetRelativeLocation(FVector(GIZMO_HALF_OFFSET, 0.0f, 0.0f));
	GizmoX->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	GizmoX->SetRelativeScale3D(FVector(0.1f, 0.1f, 1.35f));

	GizmoY = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GizmoY"));
	GizmoY->SetupAttachment(GizmoRoot);
	GizmoY->SetRelativeLocation(FVector(0.0f, GIZMO_HALF_OFFSET, 0.0f));
	GizmoY->SetRelativeRotation(FRotator(0.0f, 0.0f, -90.0f));
	GizmoY->SetRelativeScale3D(FVector(0.1f, 0.1f, 1.35f));

	GizmoZ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GizmoZ"));
	GizmoZ->SetupAttachment(GizmoRoot);
	GizmoZ->SetRelativeLocation(FVector(0.0f, 0.0f, GIZMO_HALF_OFFSET));
	GizmoZ->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
	GizmoZ->SetRelativeScale3D(FVector(0.1f, 0.1f, 1.35f));

	for (UStaticMeshComponent* Handle : {GizmoX, GizmoY, GizmoZ})
	{
		if (CylinderMeshAsset.Succeeded()) Handle->SetStaticMesh(CylinderMeshAsset.Object);

		// 注意：这里只能设"资产材质"，不能建 MaterialInstanceDynamic。
		// 构造函数里的对象会进入 CDO，CDO 若引用动态材质实例，
		// 继承本类的蓝图的保存会因 "Illegal reference to private object" 失败。
		// 红绿蓝 MID 统一放到 BeginPlay 里创建。
		if (BasicMat.Succeeded()) Handle->SetMaterial(0, BasicMat.Object);

		Handle->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Handle->SetCollisionObjectType(ECC_WorldDynamic);
		Handle->SetCollisionResponseToAllChannels(ECR_Ignore);
		Handle->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Handle->SetCastShadow(false);
	}
}

void ASplinePointMarkerBase::BeginPlay()
{
	Super::BeginPlay();

	// 运行时才创建红绿蓝材质实例（不能放在构造函数里，否则 CDO 会引用
	// MaterialInstanceEditorOnlyData，导致继承该类的蓝图无法保存）
	//ApplyHandleColors();

	ResolveSpline();
}

void ASplinePointMarkerBase::ApplyHandleColors()
{
	struct FHandleColor { UStaticMeshComponent* Handle; FLinearColor Color; };
	const FHandleColor Handles[] =
	{
		{ GizmoX, FLinearColor(1.0f, 0.1f, 0.1f) },   // 红 = X
		{ GizmoY, FLinearColor(0.1f, 0.9f, 0.2f) },   // 绿 = Y
		{ GizmoZ, FLinearColor(0.2f, 0.5f, 1.0f) },   // 蓝 = Z
	};

	for (const FHandleColor& Item : Handles)
	{
		if (!Item.Handle) continue;

		UMaterialInterface* BaseMat = Item.Handle->GetMaterial(0);
		if (!BaseMat) continue;

		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, this);
		if (!MID) continue;

		MID->SetVectorParameterValue(TEXT("Color"), Item.Color);
		Item.Handle->SetMaterial(0, MID);
	}
}

void ASplinePointMarkerBase::SetupForPoint(AActor* InTrackActor, int32 InPointIndex)
{
	TrackActor  = InTrackActor;
	PointIndex  = InPointIndex;
	ResolveSpline();
}

void ASplinePointMarkerBase::ResolveSpline()
{
	CachedSpline = TrackActor ? TrackActor->FindComponentByClass<USplineComponent>() : nullptr;
}

void ASplinePointMarkerBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC) return;
	
	
	if (bIsShow)
	{
		// 输入检测（轮询，不依赖 Enhanced Input 配置）
		if (PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))  
			OnLeftPressed();
		if (PC->WasInputKeyJustReleased(EKeys::LeftMouseButton)) 
			OnLeftReleased();
	}
	
	if (bDragging)
	{
		UpdateDrag(); 
	}

	//UpdateAppearance();
}

// =============================================================================
// 左键按下：判断点到球还是哪个手柄，建拖拽平面
// =============================================================================

void ASplinePointMarkerBase::OnLeftPressed()
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC) return;

	FHitResult Hit;
	if (!PC->GetHitResultUnderCursor(ECC_Visibility, false, Hit))
	{
		bSelected = false;
		return;
	}

	// 点到的不是自己 → 取消选中，直接返回
	if (Hit.GetActor() != this) 
	{
		bSelected = false;
		return;
	}

	const UPrimitiveComponent* HitComp = Hit.GetComponent();
	DragAxis = 0;
	if      (HitComp == GizmoX) DragAxis = 1;
	else if (HitComp == GizmoY) DragAxis = 2;
	else if (HitComp == GizmoZ) DragAxis = 3;
	// 否则 = 0，自由拖动

	// 记录：控制点位置 + 正对相机的平面
	StartActorLocation = GetActorLocation();
	DragPlaneOrigin    = StartActorLocation;

	FVector CamLoc; FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	DragPlaneNormal = CamRot.Vector();

	// 鼠标射线与该平面的交点 = 拖拽起点
	FVector RayOrigin, RayDir;
	if (!GetMouseRay(RayOrigin, RayDir)) return;

	StartHitLocation = LinePlaneIntersect(RayOrigin, RayOrigin + RayDir * 1e6f, DragPlaneOrigin, DragPlaneNormal);
	
	//UE_LOG(LogTemp,Log,TEXT("SplinePointMarkerBase::OnLeftPressed()   StartHitLocation为%s"),*StartHitLocation.ToString());
	bDragging = true;
	bSelected = true;
}

void ASplinePointMarkerBase::OnLeftReleased()
{
	bDragging = false;
	DragAxis  = 0;
}

// =============================================================================
// 拖拽更新：射线 ∩ 平面 → 位移 → 单轴投影 → 回写 Spline
// =============================================================================

void ASplinePointMarkerBase::UpdateDrag()
{
	FVector RayOrigin, RayDir;
	if (!GetMouseRay(RayOrigin, RayDir)) return;

	const FVector CurHit = LinePlaneIntersect(RayOrigin, RayOrigin + RayDir * 1e6f, DragPlaneOrigin, DragPlaneNormal);
	const FVector Delta  = CurHit- StartHitLocation;
	
	//UE_LOG(LogTemp,Log,TEXT("ASplinePointMarkerBase::UpdateDrag()   CurHit%s"),*CurHit.ToString());
	

	FVector NewLoc;
	if (DragAxis == 0)
	{
		NewLoc = StartActorLocation + Delta;
	}
	else
	{
		FVector Axis;
		switch (DragAxis)
		{
		case 1: Axis = FVector::XAxisVector; break;
		case 2: Axis = FVector::YAxisVector; break;
		case 3: Axis = FVector::ZAxisVector; break;
		default: return;
		}
		const float Proj = FVector::DotProduct(Delta, Axis);
		NewLoc = StartActorLocation + Axis * Proj;
	}

	SetActorLocation(NewLoc);

	// 写回 BP_Track 上的 Spline 控制点
	if (CachedSpline && CachedSpline->GetNumberOfSplinePoints() > PointIndex && PointIndex >= 0)
	{
		CachedSpline->SetLocationAtSplinePoint(PointIndex, NewLoc, ESplineCoordinateSpace::World, true);
	}
}

// =============================================================================
// 外观：选中才显示 Gizmo；球体/手柄按屏幕像素恒定尺寸缩放
// =============================================================================

void ASplinePointMarkerBase::UpdateAppearance()
{
	if (GizmoRoot)
	{
		GizmoRoot->SetVisibility(bSelected);
	}

	const float WorldPerPx = GetWorldPerPixelAt(GetActorLocation());

	if (SphereMesh)
	{
		const float SphereScale = FMath::Clamp(
			(WorldPerPx * MarkerScreenSizePixels) / DESIGN_SPHERE_DIAMETER, 0.02f, 1000.0f);
		//SphereMesh->SetRelativeScale3D(FVector(SphereScale));
	}

	if (GizmoRoot)
	{
		const float GizmoScale = FMath::Clamp(
			(WorldPerPx * GizmoScreenLengthPixels) / DESIGN_GIZMO_LENGTH, 0.02f, 1000.0f);
		GizmoRoot->SetRelativeScale3D(FVector(GizmoScale));
	}
}

// =============================================================================
// 工具函数
// =============================================================================

bool ASplinePointMarkerBase::GetMouseRay(FVector& OutOrigin, FVector& OutDir) const
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC) return false;
	return PC->DeprojectMousePositionToWorld(OutOrigin, OutDir);
}

FVector ASplinePointMarkerBase::LinePlaneIntersect(const FVector& LineStart, const FVector& LineEnd,
                                                   const FVector& PlaneOrigin, const FVector& PlaneNormal) const
{
	const FVector LineDir = (LineEnd - LineStart).GetSafeNormal();
	const float Dot = FVector::DotProduct(LineDir, PlaneNormal);
	if (FMath::IsNearlyZero(Dot)) return LineStart;   // 平行

	const float T = FVector::DotProduct(PlaneOrigin - LineStart, PlaneNormal) / Dot;
	return LineStart + LineDir * T;
}

float ASplinePointMarkerBase::GetWorldPerPixelAt(const FVector& WorldLoc) const
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC) return 1.0f;

	FVector CamLoc; FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);

	const float Depth = FMath::Max(1.0f, FVector::DotProduct(WorldLoc - CamLoc, CamRot.Vector()));

	float FOV = 90.0f;
	if (PC->PlayerCameraManager) FOV = PC->PlayerCameraManager->GetFOVAngle();
	if (FOV <= 0.0f) FOV = 90.0f;

	float VPHeight = 1080.0f;
	if (GEngine && GEngine->GameViewport)
	{
		FVector2D VPSize(1920, 1080);
		GEngine->GameViewport->GetViewportSize(VPSize);
		if (VPSize.Y > 0) VPHeight = VPSize.Y;
	}

	return (2.0f * Depth * FMath::Tan(FMath::DegreesToRadians(FOV * 0.5f))) / VPHeight;
}
