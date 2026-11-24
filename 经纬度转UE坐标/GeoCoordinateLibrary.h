// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GeoCoordinateLibrary.generated.h"

/**
 * 两点标定结果：把 WGS84 经纬度映射到 UE 世界坐标所需的全部参数。
 */
USTRUCT(BlueprintType)
struct FGeoCalibration
{
	GENERATED_BODY()

	/** 锚点经度（度） */
	UPROPERTY(BlueprintReadWrite, Category = "Geo")
	double OriginLongitude = 0.0;

	/** 锚点纬度（度） */
	UPROPERTY(BlueprintReadWrite, Category = "Geo")
	double OriginLatitude = 0.0;

	/** 锚点高程（米） */
	UPROPERTY(BlueprintReadWrite, Category = "Geo")
	double OriginHeight = 0.0;

	/** 锚点在 UE 世界中的位置（厘米） */
	UPROPERTY(BlueprintReadWrite, Category = "Geo")
	FVector OriginUe = FVector::ZeroVector;

	/** 场景相对正北的旋转角（度），场景未旋转时为 0 */
	UPROPERTY(BlueprintReadWrite, Category = "Geo")
	double YawDegrees = 0.0;

	/** 场景缩放，1:1 时为 1 */
	UPROPERTY(BlueprintReadWrite, Category = "Geo")
	double Scale = 1.0;

	/** 标定是否有效（两点重合时会为 false） */
	UPROPERTY(BlueprintReadWrite, Category = "Geo")
	bool bValid = false;
};

/**
 * 经纬度 <-> UE 局部坐标换算（纯数学，不依赖 Cesium）。
 *
 * 约定与 Cesium for Unreal 一致：原点为局部东北天(ENU)坐标的站点，
 * UE 世界坐标轴 X = 东、Y = 南、Z = 上，单位为厘米。
 */
UCLASS()
class NC_GDS_API UGeoCoordinateLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	 * WGS84 经纬高 -> UE 世界坐标（完整算法：经纬度 -> ECEF -> ENU -> UE）。
	 * 精度在厘米级，适合几十公里范围。
	 *
	 * @param Longitude      目标经度（度）
	 * @param Latitude       目标纬度（度）
	 * @param Height         目标高程（米）
	 * @param OriginLongitude 原点经度（度），一般取场景中心
	 * @param OriginLatitude  原点纬度（度）
	 * @param OriginHeight    原点高程（米）
	 * @return UE 坐标（厘米），原点处为 (0,0,0)
	 */
	UFUNCTION(BlueprintPure, Category = "Geo")
	static FVector LonLatHeightToUe(
		double Longitude, double Latitude, double Height,
		double OriginLongitude, double OriginLatitude, double OriginHeight);

	/**
	 * WGS84 经纬高 -> UE 世界坐标（等距圆柱近似，经度按原点纬度做余弦收敛）。
	 * 3 公里范围内误差小于 1 米，计算量最小，适合每帧调用。
	 */
	UFUNCTION(BlueprintPure, Category = "Geo")
	static FVector LonLatHeightToUeApprox(
		double Longitude, double Latitude, double Height,
		double OriginLongitude, double OriginLatitude, double OriginHeight);

	/**
	 * 用两个同名点做标定：A、B 分别为 2D 图与 UE 场景中同一个地物的经纬度和 UE 坐标。
	 * 输出锚点、旋转角与缩放，可直接用于 ApplyCalibration。
	 */
	UFUNCTION(BlueprintPure, Category = "Geo")
	static FGeoCalibration CalibrateFromTwoPoints(
		double LonA, double LatA, double HeightA, FVector UeA,
		double LonB, double LatB, double HeightB, FVector UeB);

	/**
	 * 用标定结果把经纬度换算成 UE 世界坐标（厘米）。
	 */
	UFUNCTION(BlueprintPure, Category = "Geo")
	static FVector ApplyCalibration(
		double Longitude, double Latitude, double Height,
		const FGeoCalibration& Calibration);
};
