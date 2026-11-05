// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SplinePointMarkerBase.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class USplineComponent;

/**
 * 样条控制点小球 Actor —— 自带 XYZ Gizmo 拖拽逻辑。
 *
 * 用法：BP_SplinePointMarker 的父类改成本类即可（BP_Track 不需要改父类）。
 *   - C++ 会自动创建：球体根组件 + GizmoRoot + 红/绿/蓝三根圆柱手柄
 *   - 左键点小球 = 自由拖动；点 X/Y/Z 手柄 = 单轴拖动
 *   - 拾取/手柄长度按屏幕像素恒定，远距离也能点中
 *   - TrackActor 填任意带 SplineComponent 的 Actor（比如 BP_Track），
 *     拖动时会自动把该组件的第 PointIndex 个控制点写回
 *
 * 不需要连任何蓝图节点。
 */
UCLASS(Blueprintable)
class QIYUAN_API ASplinePointMarkerBase : public AActor
{
	GENERATED_BODY()

public:
	ASplinePointMarkerBase();

	// ========== 组件 ==========

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Marker")
	TObjectPtr<UStaticMeshComponent> SphereMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Marker")
	TObjectPtr<USceneComponent> GizmoRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Marker")
	TObjectPtr<UStaticMeshComponent> GizmoX;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Marker")
	TObjectPtr<UStaticMeshComponent> GizmoY;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Marker")
	TObjectPtr<UStaticMeshComponent> GizmoZ;

	// ========== 配置 ==========

	/** 这个小球对应 Spline 上的第几个控制点 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker")
	int32 PointIndex = -1;

	/** 指向带 SplineComponent 的 Actor（BP_Track），用来回写控制点位置 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker")
	TObjectPtr<AActor> TrackActor;

	/** 小球在屏幕上的目标直径（像素）—— 恒定值，多远都一样大 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker", meta = (ClampMin = "2.0"))
	float MarkerScreenSizePixels = 12.0f;

	/** Gizmo 手柄在屏幕上的长度（像素） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker", meta = (ClampMin = "10.0"))
	float GizmoScreenLengthPixels = 90.0f;

	// ========== 运行时状态 ==========

	/** 是否被选中（选中才显示 XYZ 手柄） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Marker")
	bool bSelected = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Marker")
	bool bDragging = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Marker")
	int32 DragAxis = 0;   // 0=自由 1=X 2=Y 3=Z

	/** 由外部（SplineMarkerSpawner）设置：初始化目标样条 */
	UFUNCTION(BlueprintCallable, Category = "Marker")
	void SetupForPoint(AActor* InTrackActor, int32 InPointIndex);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ---- 拖拽内部 ----
	void OnLeftPressed();
	void OnLeftReleased();
	void UpdateDrag();
	void UpdateAppearance();

	// ---- 工具 ----
	void  ApplyHandleColors();
	bool  GetMouseRay(FVector& OutOrigin, FVector& OutDir) const;
	FVector LinePlaneIntersect(const FVector& LineStart, const FVector& LineEnd,
	                           const FVector& PlaneOrigin, const FVector& PlaneNormal) const;
	float GetWorldPerPixelAt(const FVector& WorldLoc) const;
	void  ResolveSpline();

	/** 缓存的 BP_Track 上的 Spline 组件 */
	UPROPERTY(Transient)
	TObjectPtr<USplineComponent> CachedSpline;

	// 拖拽起点状态
	FVector DragPlaneOrigin    = FVector::ZeroVector;
	FVector DragPlaneNormal    = FVector::ForwardVector;
	FVector StartHitLocation   = FVector::ZeroVector;
	FVector StartActorLocation = FVector::ZeroVector;
	
	
public:
	UPROPERTY(BlueprintReadWrite)
	bool bIsShow=false;
};
