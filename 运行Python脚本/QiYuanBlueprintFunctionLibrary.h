// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "QiYuanBlueprintFunctionLibrary.generated.h"

/**
 * 
 */
UCLASS()
class QIYUAN_API UQiYuanBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	// 暴露给蓝图调用的函数
	UFUNCTION(BlueprintCallable, Category = "Python")
	static void RunPythonScript(const FString& ScriptPath, const FString& Args);
};
