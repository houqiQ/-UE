// Fill out your copyright notice in the Description page of Project Settings.

#include "GeoCoordinateLibrary.h"

namespace Geo
{
	// WGS84 椭球参数
	constexpr double SemiMajorAxis = 6378137.0;              // 长半轴 a（米）
	constexpr double FirstEccentricitySquared = 0.00669437999014; // e^2
	constexpr double MetersToCentimeters = 100.0;

	// 度 -> 米 的近似换算（用于等距圆柱近似）
	constexpr double MetersPerDegreeLatitude = 110540.0;
	constexpr double MetersPerDegreeLongitudeAtEquator = 111320.0;

	// 经纬高 -> ECEF（地心地固坐标，米）
	FVector EcefFromLonLatHeight(double Longitude, double Latitude, double Height)
	{
		const double RadLon = FMath::DegreesToRadians(Longitude);
		const double RadLat = FMath::DegreesToRadians(Latitude);
		const double SinLat = FMath::Sin(RadLat);
		const double CosLat = FMath::Cos(RadLat);

		// 卯酉圈曲率半径
		const double N = SemiMajorAxis / FMath::Sqrt(1.0 - FirstEccentricitySquared * SinLat * SinLat);

		return FVector(
			(N + Height) * CosLat * FMath::Cos(RadLon),
			(N + Height) * CosLat * FMath::Sin(RadLon),
			(N * (1.0 - FirstEccentricitySquared) + Height) * SinLat);
	}
}

FVector UGeoCoordinateLibrary::LonLatHeightToUe(
	double Longitude, double Latitude, double Height,
	double OriginLongitude, double OriginLatitude, double OriginHeight)
{
	const FVector Delta = Geo::EcefFromLonLatHeight(Longitude, Latitude, Height)
		- Geo::EcefFromLonLatHeight(OriginLongitude, OriginLatitude, OriginHeight);

	const double SinLon = FMath::Sin(FMath::DegreesToRadians(OriginLongitude));
	const double CosLon = FMath::Cos(FMath::DegreesToRadians(OriginLongitude));
	const double SinLat = FMath::Sin(FMath::DegreesToRadians(OriginLatitude));
	const double CosLat = FMath::Cos(FMath::DegreesToRadians(OriginLatitude));

	// ECEF 差值 -> 站心 ENU（东、北、天）
	const double East = -SinLon * Delta.X + CosLon * Delta.Y;
	const double North = -SinLat * CosLon * Delta.X - SinLat * SinLon * Delta.Y + CosLat * Delta.Z;
	const double Up = CosLat * CosLon * Delta.X + CosLat * SinLon * Delta.Y + SinLat * Delta.Z;

	// ENU -> UE：X = 东，Y = 南，Z = 上
	return FVector(East, -North, Up) * Geo::MetersToCentimeters;
}

FVector UGeoCoordinateLibrary::LonLatHeightToUeApprox(
	double Longitude, double Latitude, double Height,
	double OriginLongitude, double OriginLatitude, double OriginHeight)
{
	const double East = (Longitude - OriginLongitude)
		* Geo::MetersPerDegreeLongitudeAtEquator
		* FMath::Cos(FMath::DegreesToRadians(OriginLatitude));
	const double North = (Latitude - OriginLatitude) * Geo::MetersPerDegreeLatitude;
	const double Up = Height - OriginHeight;

	return FVector(East, -North, Up) * Geo::MetersToCentimeters;
}

FGeoCalibration UGeoCoordinateLibrary::CalibrateFromTwoPoints(
	double LonA, double LatA, double HeightA, FVector UeA,
	double LonB, double LatB, double HeightB, FVector UeB)
{
	FGeoCalibration Out;
	Out.OriginLongitude = LonA;
	Out.OriginLatitude = LatA;
	Out.OriginHeight = HeightA;
	Out.OriginUe = UeA;

	// A->B 在经纬度空间中的偏移（米，UE 轴系：X=东，Y=南）
	const FVector Geographic = LonLatHeightToUe(LonB, LatB, HeightB, LonA, LatA, HeightA)
		/ Geo::MetersToCentimeters;
	// A->B 在 UE 场景中的实际偏移（米）
	const FVector Actual = (UeB - UeA) / Geo::MetersToCentimeters;

	const double GeographicLength = Geographic.Size2D();
	const double ActualLength = Actual.Size2D();
	if (GeographicLength < 1.0 || ActualLength < 1.0)
	{
		// 两点太近，无法解算旋转和缩放
		return Out;
	}

	const double GeographicAngle = FMath::Atan2(Geographic.Y, Geographic.X);
	const double ActualAngle = FMath::Atan2(Actual.Y, Actual.X);

	Out.YawDegrees = FMath::RadiansToDegrees(ActualAngle - GeographicAngle);
	Out.Scale = ActualLength / GeographicLength;
	Out.bValid = true;
	return Out;
}

FVector UGeoCoordinateLibrary::ApplyCalibration(
	double Longitude, double Latitude, double Height,
	const FGeoCalibration& Calibration)
{
	if (!Calibration.bValid)
	{
		return Calibration.OriginUe;
	}

	// 相对锚点的偏移（厘米，X=东，Y=南，Z=上）
	const FVector Relative = LonLatHeightToUe(
		Longitude, Latitude, Height,
		Calibration.OriginLongitude, Calibration.OriginLatitude, Calibration.OriginHeight);

	// 绕 Z 轴旋转到场景朝向，再按标定比例缩放
	const double RadYaw = FMath::DegreesToRadians(Calibration.YawDegrees);
	const double CosYaw = FMath::Cos(RadYaw);
	const double SinYaw = FMath::Sin(RadYaw);

	const double RotatedX = Relative.X * CosYaw - Relative.Y * SinYaw;
	const double RotatedY = Relative.X * SinYaw + Relative.Y * CosYaw;

	return Calibration.OriginUe + FVector(RotatedX, RotatedY, Relative.Z) * Calibration.Scale;
}
