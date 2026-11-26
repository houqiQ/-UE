// Copyright 2024 QiYuan. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NetLanReceiverComponent.generated.h"

/** 局部坐标原点（二维端 hello.origin），ENU 投影基准 */
USTRUCT(BlueprintType)
struct FTelemetryOrigin
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	double Lon = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	double Lat = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	double Alt = 0.0;
};

/** 连接握手信息（m=hello），二维端在 UE 连上后立刻下发 */
USTRUCT(BlueprintType)
struct FTelemetryHello
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	int32 Version = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	FString Server;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	FString Frame;

	/** enu = 消息里带米制局部坐标 x/y/z；wgs84 = 只有经纬度 */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	FString Coord;

	/** north_x: X=北 Y=东；east_x: X=东 Y=北 */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	FString Axes;

	/** m 或 cm */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	FString LengthUnit;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	FTelemetryOrigin Origin;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	bool bHasOrigin = false;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	int32 Tick = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	int32 XSpeed = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	int32 DetailIntervalMs = 1000;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	FString SimStart;

	/** UTC 毫秒时间戳 */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	double Timestamp = 0.0;
};

/** 推演时钟（m=tick） */
USTRUCT(BlueprintType)
struct FTelemetryTick
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	int32 Tick = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	int32 XSpeed = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	FString SimStart;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	double Timestamp = 0.0;
};

/** 实体状态（m=unit） */
USTRUCT(BlueprintType)
struct FTelemetryUnit
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	int32 Party = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	FString Army;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	FString ArmyName;

	/** 阵营数字枚举：友方=1、敌方=0、中立方=2、未知=3 */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	int32 Side = 3;

	/** 阵营英文枚举：friendly / enemy / neutral / unknown */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	FString Diplomacy;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	FString Kind;

	/** 实体=0、聚合体=1、编队=2、阵营=3 */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	int32 ObjType = 0;

	/** 3d_model：实体型号（未设置时为空） */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	FString Model3D;

	/** 3d_category：实体装备类别 */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	FString Category3D;

	/** army_model：所属聚合体型号 */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	FString ArmyModel;

	/** army_category：所属聚合体装备类别 */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	FString ArmyCategory;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	bool bHasLonLat = false;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	double Lon = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	double Lat = 0.0;

	/** 海拔（米） */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	double Alt = 0.0;

	/** ENU 局部坐标（米），仅 coord=enu 时下发 */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	bool bHasXYZ = false;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	double X = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	double Y = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	double Z = 0.0;

	/** 航向（0=正北，顺时针，可直接当 Yaw） */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	double Heading = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	double Speed = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	double Height = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	bool bAlive = true;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	bool bPrisoner = false;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	bool bLeader = false;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	int32 State = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	int32 Life = 100;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	int32 Tick = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	int32 XSpeed = 1;

	/** detail=1 表示本次带人员/装备/物资明细 */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	bool bHasDetail = false;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	double Timestamp = 0.0;

	/** 原始 JSON，供蓝图自行取更多字段 */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	FString RawJson;
};

/** 收到文本消息（已按 UTF-8 解码，自动去掉结尾的 \0 / \r / \n） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FNetMessageReceived, const FString&, Message, const FString&, SenderIP, int32, SenderPort);

/** 收到原始字节（二进制数据用这个） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FNetBytesReceived, const TArray<uint8>&, Data, const FString&, SenderIP, int32, SenderPort);

/** 与二维端 TCP 连接状态变化 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTelemetryConnectedChanged, bool, bConnected);

/** m=hello 握手 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTelemetryHelloReceived, const FTelemetryHello&, Hello);

/** m=tick 推演时钟 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTelemetryTickReceived, const FTelemetryTick&, TickInfo);

/** m=unit 实体状态 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTelemetryUnitReceived, const FTelemetryUnit&, Unit);

/** 倍速变化（hello/tick/unit/control_speed 中任一携带 x_speed 时触发） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTelemetrySpeedChanged, int32, XSpeed);

/** 推演运行状态变化（control_state：0=运行 1=暂停） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTelemetrySimStateChanged, int32, SimState);

/** 视角聚焦（control_focus） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTelemetryFocusChanged, const FString&, Id, const FString&, Name);

/** 其它任意 JSON 消息（fire/fire_end/effect/effect_end/path/vision/report 等） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FTelemetryJsonReceived, const FString&, MessageType, const FString&, Json, const FString&, SenderIP, int32, SenderPort);

/** 使用的网络协议 */
UENUM(BlueprintType, meta = (DisplayName = "协议"))
enum class ENetProtocol : uint8
{
	UDP		UMETA(DisplayName = "UDP"),
	TCP		UMETA(DisplayName = "TCP"),
	Both	UMETA(DisplayName = "同时监听 UDP+TCP")
};

/** TCP 角色：UE 主动连二维端（客户端）还是自己当服务器 */
UENUM(BlueprintType, meta = (DisplayName = "TCP 角色"))
enum class ETcpMode : uint8
{
	Client	UMETA(DisplayName = "客户端（主动连二维端）"),
	Server	UMETA(DisplayName = "服务器（等别人连进来）")
};

/** TCP 消息分帧方式 */
UENUM(BlueprintType, meta = (DisplayName = "TCP 分帧"))
enum class ETcpFraming : uint8
{
	Delimiter		UMETA(DisplayName = "分隔符（文本协议，如 \\n）"),
	LengthPrefix	UMETA(DisplayName = "长度前缀（4 字节长度头 + JSON，二维端默认）")
};

/** 长度头的字节序（二维端为小端） */
UENUM(BlueprintType, meta = (DisplayName = "长度头字节序"))
enum class ELengthByteOrder : uint8
{
	LittleEndian	UMETA(DisplayName = "小端 LE（二维端默认，x86）"),
	BigEndian		UMETA(DisplayName = "大端 BE（网络序）")
};

/**
 * LAN 网络收发组件。
 *
 * 默认走「UE 作为 TCP 客户端，主动连接二维端服务器」：
 *   二维端 DataAnalysisRecorder 默认监听 7770（JOSS 7771、SimConsole 7772），
 *   UE 连上后二维端立刻下发 hello，随后持续推送 tick / unit / fire / effect 等 JSON。
 *
 * 帧格式（长度前缀模式，小端）：
 *   [4 字节小端长度 N][N 字节 UTF-8 JSON]
 *   一次 recv 可能包含多条消息，组件已循环解帧。
 *
 * 用法：蓝图里「添加组件」→ 搜索 "LAN 网络收发" → 填 Server IP / Server Port → Play。
 */
UCLASS(ClassGroup = (QiYuan), meta = (BlueprintSpawnableComponent, DisplayName = "LAN 网络收发 (遥测)"))
class QIYUAN_API UNetLanReceiverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNetLanReceiverComponent();

	// ======================== 配置 ========================

	/** 协议：UDP / TCP / 同时监听 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "网络",
		meta = (DisplayName = "协议 Protocol"))
	ENetProtocol Protocol = ENetProtocol::TCP;

	/** TCP 角色：接二维端数据时选「客户端」 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "网络",
		meta = (DisplayName = "TCP 角色", EditCondition = "Protocol != ENetProtocol::UDP"))
	ETcpMode TcpMode = ETcpMode::Client;

	/** 二维端所在机器的 IP */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "网络",
		meta = (DisplayName = "服务器 IP", EditCondition = "Protocol != ENetProtocol::UDP && TcpMode == ETcpMode::Client"))
	FString ServerIP = TEXT("127.0.0.1");

	/** 二维端监听端口（DataAnalysisRecorder 默认 7770） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "网络",
		meta = (DisplayName = "服务器端口", ClampMin = "1", ClampMax = "65535",
			EditCondition = "Protocol != ENetProtocol::UDP && TcpMode == ETcpMode::Client"))
	int32 ServerPort = 7770;

	/** 断线后自动重连 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "网络",
		meta = (DisplayName = "断线自动重连", EditCondition = "Protocol != ENetProtocol::UDP && TcpMode == ETcpMode::Client"))
	bool bAutoReconnect = true;

	/** 重连间隔（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "网络",
		meta = (DisplayName = "重连间隔(秒)", ClampMin = "0.5", ClampMax = "60.0",
			EditCondition = "bAutoReconnect && Protocol != ENetProtocol::UDP && TcpMode == ETcpMode::Client"))
	float ReconnectInterval = 3.0f;

	/** TCP 分帧方式（二维端默认长度前缀） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "网络",
		meta = (DisplayName = "TCP 分帧", EditCondition = "Protocol != ENetProtocol::UDP"))
	ETcpFraming TcpFraming = ETcpFraming::LengthPrefix;

	/** 长度头字节序（二维端为小端） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "网络",
		meta = (DisplayName = "长度头字节序", EditCondition = "Protocol != ENetProtocol::UDP && TcpFraming == ETcpFraming::LengthPrefix"))
	ELengthByteOrder LengthByteOrder = ELengthByteOrder::LittleEndian;

	/** 分隔符模式下用什么切消息（默认 \n） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "网络",
		meta = (DisplayName = "分隔符", EditCondition = "TcpFraming == ETcpFraming::Delimiter"))
	FString Delimiter = TEXT("\n");

	/** 单帧最大字节数，超过视为协议错乱并断开 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "网络",
		meta = (DisplayName = "单帧最大字节", ClampMin = "1024", ClampMax = "67108864"))
	int32 MaxFrameSize = 512 * 1024;

	/** 服务器模式下的监听端口 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "网络",
		meta = (DisplayName = "监听端口（服务器模式）", ClampMin = "1", ClampMax = "65535",
			EditCondition = "Protocol != ENetProtocol::UDP && TcpMode == ETcpMode::Server"))
	int32 ListenPort = 9000;

	/** BeginPlay 时自动启动（连接 / 监听） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "网络",
		meta = (DisplayName = "BeginPlay 自动启动"))
	bool bAutoStart = true;

	/** 每个 Tick 最多处理多少条消息 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "网络",
		meta = (DisplayName = "每帧最多处理消息数", ClampMin = "1", ClampMax = "4096"))
	int32 MaxPacketsPerTick = 512;

	/** 是否把收到的每条消息打进 Output Log */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "网络",
		meta = (DisplayName = "打印日志"))
	bool bLogMessages = true;

	/** unit 消息频率高，单独开关（默认关，避免日志刷爆） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "网络",
		meta = (DisplayName = "打印单位日志", EditCondition = "bLogMessages"))
	bool bLogUnitMessages = false;

	// ======================== 生命周期 API ========================

	/** 按配置启动：UDP 监听 / TCP 监听（服务器） / 连接二维端（客户端） */
	UFUNCTION(BlueprintCallable, Category = "网络")
	bool StartListening();

	/** 停止并释放所有 socket */
	UFUNCTION(BlueprintCallable, Category = "网络")
	void StopListening();

	/** 主动连接二维端（客户端模式） */
	UFUNCTION(BlueprintCallable, Category = "网络")
	bool ConnectToServer();

	/** 主动断开与二维端的连接 */
	UFUNCTION(BlueprintCallable, Category = "网络")
	void DisconnectFromServer();

	/** 是否已启动（客户端模式下代表"已在连接/已连接"） */
	UFUNCTION(BlueprintPure, Category = "网络")
	bool IsListening() const { return bStarted; }

	/** 是否已与二维端建立 TCP 连接 */
	UFUNCTION(BlueprintPure, Category = "网络")
	bool IsConnected() const { return ClientSocket != nullptr; }

	/** 服务器模式下实际监听端口 */
	UFUNCTION(BlueprintPure, Category = "网络")
	int32 GetLocalPort() const { return BoundPort; }

	/** 二维端地址（IP:Port） */
	UFUNCTION(BlueprintPure, Category = "网络")
	FString GetServerAddress() const;

	/** 当前协议名 */
	UFUNCTION(BlueprintPure, Category = "网络")
	FString GetProtocolName() const;

	/** 当前 TCP 连接数 */
	UFUNCTION(BlueprintPure, Category = "网络")
	int32 GetTcpConnectionCount() const;

	// ======================== 发送 ========================

	/** 发送 UTF-8 文本（TCP 自动加分帧；UDP 直接发） */
	UFUNCTION(BlueprintCallable, Category = "网络")
	bool SendString(const FString& Message, const FString& TargetIP, int32 TargetPort);

	/** 发送原始字节 */
	UFUNCTION(BlueprintCallable, Category = "网络")
	bool SendBytes(const TArray<uint8>& Data, const FString& TargetIP, int32 TargetPort);

	/** 回复最近一次的来源（客户端模式下即发回二维端） */
	UFUNCTION(BlueprintCallable, Category = "网络")
	bool SendStringToLastSender(const FString& Message);

	/** 回复原始字节给最近一次的来源 */
	UFUNCTION(BlueprintCallable, Category = "网络")
	bool SendBytesToLastSender(const TArray<uint8>& Data);

	/** 最近一次收到数据的来源 IP */
	UFUNCTION(BlueprintPure, Category = "网络")
	FString GetLastSenderIP() const { return LastSenderIP; }

	/** 最近一次收到数据的来源端口 */
	UFUNCTION(BlueprintPure, Category = "网络")
	int32 GetLastSenderPort() const { return LastSenderPort; }

	// ======================== 数据查询 ========================

	/** 最近一次 hello 是否已收到 */
	UFUNCTION(BlueprintPure, Category = "遥测")
	bool HasHello() const { return bHasHello; }

	/** 最近一次 hello */
	UFUNCTION(BlueprintPure, Category = "遥测")
	FTelemetryHello GetHello() const { return CachedHello; }

	/** 按 ID 取该实体最近一次状态 */
	UFUNCTION(BlueprintPure, Category = "遥测")
	bool GetUnitById(const FString& Id, FTelemetryUnit& OutUnit) const;

	/** 所有已收到的实体最新状态 */
	UFUNCTION(BlueprintPure, Category = "遥测")
	TArray<FTelemetryUnit> GetAllUnits() const;

	/** 已收到的实体数量 */
	UFUNCTION(BlueprintPure, Category = "遥测")
	int32 GetUnitCount() const { return UnitMap.Num(); }

	/** 清空实体缓存 */
	UFUNCTION(BlueprintCallable, Category = "遥测")
	void ClearUnits();

	/** 从任意 JSON 取字符串字段 */
	UFUNCTION(BlueprintPure, Category = "遥测|JSON")
	static FString JsonGetString(const FString& Json, const FString& Key);

	/** 从任意 JSON 取数字字段 */
	UFUNCTION(BlueprintPure, Category = "遥测|JSON")
	static float JsonGetNumber(const FString& Json, const FString& Key);

	// ======================== 事件 ========================

	/** 收到文本消息（所有消息都会触发） */
	UPROPERTY(BlueprintAssignable, Category = "网络|事件")
	FNetMessageReceived OnMessageReceived;

	/** 收到原始字节 */
	UPROPERTY(BlueprintAssignable, Category = "网络|事件")
	FNetBytesReceived OnBytesReceived;

	/** 与二维端连接状态变化 */
	UPROPERTY(BlueprintAssignable, Category = "网络|事件")
	FTelemetryConnectedChanged OnConnectedChanged;

	/** m=hello */
	UPROPERTY(BlueprintAssignable, Category = "网络|事件")
	FTelemetryHelloReceived OnHelloReceived;

	/** m=tick */
	UPROPERTY(BlueprintAssignable, Category = "网络|事件")
	FTelemetryTickReceived OnTickReceived;

	/** m=unit */
	UPROPERTY(BlueprintAssignable, Category = "网络|事件")
	FTelemetryUnitReceived OnUnitReceived;

	/** 倍速变化 */
	UPROPERTY(BlueprintAssignable, Category = "网络|事件")
	FTelemetrySpeedChanged OnSpeedChanged;

	/** 推演状态变化（0=运行 1=暂停） */
	UPROPERTY(BlueprintAssignable, Category = "网络|事件")
	FTelemetrySimStateChanged OnSimStateChanged;

	/** 视角聚焦 */
	UPROPERTY(BlueprintAssignable, Category = "网络|事件")
	FTelemetryFocusChanged OnFocusChanged;

	/** 其它 JSON 消息（fire/effect/path/vision/report 等） */
	UPROPERTY(BlueprintAssignable, Category = "网络|事件")
	FTelemetryJsonReceived OnJsonMessage;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	// ---- 底层启动/停止 ----
	bool StartUdpListening();
	bool StartTcpListening();
	void StopUdpListening();
	void StopTcpListening();
	void CloseClientSocket();

	// ---- 轮询 ----
	void PollAll(float DeltaTime);
	void PollUdp();
	void PollTcpAccept();
	void PollTcpServerClients();
	void PollTcpClient(float DeltaTime);
	void CleanupDisconnectedTcpClients();

	// ---- 分帧（服务器/客户端共用）----
	int32 ProcessReceiveBuffer(TArray<uint8>& Buffer, const FString& SenderIP, int32 SenderPort, bool& bOutShouldClose);

	// ---- 消息分发 ----
	void DispatchMessage(const TArray<uint8>& Data, const FString& SenderIP, int32 SenderPort);
	void TrimAndDispatchText(const TArray<uint8>& Data, const FString& SenderIP, int32 SenderPort);
	void HandleJsonMessage(const FString& Json, const FString& SenderIP, int32 SenderPort);

	// ---- 发送 ----
	bool SendBytesUdp(const TArray<uint8>& Data, const FString& TargetIP, int32 TargetPort);
	bool SendBytesTcp(const TArray<uint8>& Data, const FString& TargetIP, int32 TargetPort);
	void ApplyFraming(const TArray<uint8>& Data, TArray<uint8>& OutBuffer) const;

	// ---- 数据 ----
	class FSocket* UdpSocket = nullptr;
	class FSocket* TcpListenSocket = nullptr;
	class FSocket* ClientSocket = nullptr;

	struct FTcpClient
	{
		FSocket* Socket = nullptr;
		TArray<uint8> RecvBuffer;
		FString SenderIP;
		int32 SenderPort = 0;
		bool bDisconnected = false;
	};
	TArray<FTcpClient*> TcpClients;

	TArray<uint8> ClientRecvBuffer;
	float ReconnectCountdown = 0.0f;

	bool bStarted = false;
	int32 BoundPort = 0;

	FString LastSenderIP;
	int32 LastSenderPort = 0;
	bool bHasLastSender = false;

	// ---- 解析结果缓存 ----
	FTelemetryHello CachedHello;
	bool bHasHello = false;
	TMap<FString, FTelemetryUnit> UnitMap;
	int32 LastSpeed = 1;
	bool bSpeedKnown = false;
};
