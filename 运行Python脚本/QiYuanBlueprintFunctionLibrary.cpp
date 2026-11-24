// Fill out your copyright notice in the Description page of Project Settings.


#include "QiYuanBlueprintFunctionLibrary.h"
#include "Misc/Paths.h"

void UQiYuanBlueprintFunctionLibrary::RunPythonScript(const FString& ScriptPath, const FString& Args)
{
	// CreateProc 签名:
	//   URL  = 可执行文件路径
	//   Parms = 命令行参数 (传给可执行文件)
	const FString PythonExe = TEXT("C:/Users/openebridge/miniconda3/envs/pecs/python.exe");

	// 工作目录 = 项目目录下的 EventCamera 文件夹 (python 脚本都在这)
	// 编辑器: D:/New/QiYuan/EventCamera    打包: Windows/QiYuan/EventCamera
	const FString WorkDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("EventCamera"));

	// 统一把脚本解析到 <ProjectDir>/EventCamera/<文件名>:
	// 蓝图里存的是绝对路径(指向源码工程), 打包后会跑错目录, 这里按文件名重定向
	FString ResolvedScript = ScriptPath;
	{
		const FString InEventCamera = WorkDir / FPaths::GetCleanFilename(ScriptPath);
		if (FPaths::FileExists(InEventCamera))
		{
			ResolvedScript = InEventCamera;
		}
	}

	// Parms: 脚本路径 + 蓝图传的 args
	const FString Parms = Args.IsEmpty()
		? FString::Printf(TEXT("\"%s\""), *ResolvedScript)
		: FString::Printf(TEXT("\"%s\" %s"), *ResolvedScript, *Args);

	uint32 OutProcessID = 0;
	FProcHandle ProcHandle = FPlatformProcess::CreateProc(
		*PythonExe,
		*Parms,
		true,              // bLaunchDetached: UE PIE 停了进程继续跑
		false,             // bLaunchHidden: false → 显示黑框, 方便看输出
		false,
		&OutProcessID,
		0,
		*WorkDir,
		nullptr,
		nullptr
	);

	UE_LOG(LogTemp, Log, TEXT("已启动 Python: 进程ID=%d, workdir=%s, parms=%s (句柄有效=%s)"),
		OutProcessID, *WorkDir, *Parms, ProcHandle.IsValid() ? TEXT("true") : TEXT("false"));
}
