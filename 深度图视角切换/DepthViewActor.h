#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DepthViewActor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class APostProcessVolume;

/**
 * 运行时把主画面切换成深度图效果（近亮远暗灰度）。
 * 用法：把该 Actor 拖进关卡，运行后按 F8 切换；蓝图也可调用开关 / 调最远距离。
 * 原理：生成一个 PostProcess 材质（SceneDepth -> 取 R -> 除 MaxDepth -> 1-x -> 灰度输出），
 *       挂到一个 Unbound 的 PostProcessVolume 上，通过 weight 0/1 开关。
 * 打包注意：优先使用 DepthMaterial 指定的资源（可随包烘焙，打包版本必须用这种）；
 *           未指定时只在编辑器/PIE 里运行时临时生成（打包版没有 shader 编译器，无法生效）。
 */
UCLASS()
class ADepthViewActor : public AActor
{
    GENERATED_BODY()

public:
    ADepthViewActor();

    virtual void Tick(float DeltaSeconds) override;

    /** 切换深度图显示 / 还原正常画面（默认 F8 触发，蓝图也可调用） */
    UFUNCTION(BlueprintCallable, Category = "Depth View")
    void ToggleDepthView();

    /** 直接开关深度图 */
    UFUNCTION(BlueprintCallable, Category = "Depth View")
    void SetDepthViewEnabled(bool bEnable);

    /** 设置深度可视化最远距离（cm），超过此距离显示纯黑。默认 7000 = 70m */
    UFUNCTION(BlueprintCallable, Category = "Depth View")
    void SetDepthMaxDistance(float MaxDistanceCm);

#if WITH_EDITORONLY_DATA
    /** 在编辑器里烘焙并保存深度图材质资源到 /Game/DepthView/M_DepthView（打包前点一次） */
    UFUNCTION(CallInEditor, Category = "Depth View")
    void CreateAndSaveDepthMaterial();
#endif

    /** 当前是否显示深度图 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Depth View")
    bool bDepthViewEnabled = false;

    /** 深度可视化最远距离（cm），和导出的 _depth_vis.png 口径一致 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Depth View", meta = (ClampMin = "100.0"))
    float DepthMaxDistanceCm = 70000.0f;

    /** 可选：指定烘焙好的深度图材质资源。留空则在编辑器/PIE 里运行时临时生成。
     *  打包版本必须用这个（先用 CreateAndSaveDepthMaterial 烘焙，再赋到这里）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Depth View")
    TSoftObjectPtr<UMaterialInterface> DepthMaterial;

protected:
    virtual void BeginPlay() override;

private:
    /** 运行时构建深度后处理材质 + 创建 Unbound PostProcessVolume */
    void BuildDepthMaterial();
    /** 把当前开关状态应用到 blendable weight */
    void ApplyBlendWeight();

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> DepthMID;

    UPROPERTY(Transient)
    TObjectPtr<APostProcessVolume> DepthPPVolume;

    bool bInputBound = false;
};
