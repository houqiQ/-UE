// Copyright 2024 QiYuan. All Rights Reserved.

#include "NetLanReceiverComponent.h"

#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Common/UdpSocketBuilder.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "IPAddress.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogNetLan, Log, All);

namespace
{
	constexpr int32 UdpMaxDatagramSize = 65507;
	constexpr int32 TcpRecvChunkSize   = 64 * 1024;

	// ---------- JSON 取值小工具：字段缺失时返回默认值 ----------
	double JsonNumber(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, double Default)
	{
		double Value = Default;
		if (Obj.IsValid() && Obj->TryGetNumberField(FString(Key), Value))
		{
			return Value;
		}
		return Default;
	}

	int32 JsonInt(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, int32 Default)
	{
		double Value = Default;
		if (Obj.IsValid() && Obj->TryGetNumberField(FString(Key), Value))
		{
			return static_cast<int32>(FMath::RoundToInt(Value));
		}
		return Default;
	}

	FString JsonString(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, const FString& Default = FString())
	{
		FString Value;
		if (Obj.IsValid() && Obj->TryGetStringField(FString(Key), Value))
		{
			return Value;
		}
		return Default;
	}

	bool JsonBool(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, bool Default)
	{
		bool Value = Default;
		if (Obj.IsValid() && Obj->TryGetBoolField(FString(Key), Value))
		{
			return Value;
		}
		return Default;
	}
}

UNetLanReceiverComponent::UNetLanReceiverComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

FString UNetLanReceiverComponent::GetProtocolName() const
{
	switch (Protocol)
	{
	case ENetProtocol::UDP:  return TEXT("UDP");
	case ENetProtocol::TCP:  return TEXT("TCP");
	case ENetProtocol::Both: return TEXT("UDP+TCP");
	}
	return TEXT("UDP");
}

FString UNetLanReceiverComponent::GetServerAddress() const
{
	return FString::Printf(TEXT("%s:%d"), *ServerIP, ServerPort);
}

int32 UNetLanReceiverComponent::GetTcpConnectionCount() const
{
	if (TcpMode == ETcpMode::Client)
	{
		return ClientSocket != nullptr ? 1 : 0;
	}
	return TcpClients.Num();
}

// ==================== 生命周期 ====================

void UNetLanReceiverComponent::BeginPlay()
{
	Super::BeginPlay();
	if (bAutoStart)
	{
		StartListening();
	}
}

void UNetLanReceiverComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopListening();
	Super::EndPlay(EndPlayReason);
}

// ==================== 启动 / 停止 ====================

bool UNetLanReceiverComponent::StartListening()
{
	if (bStarted)
	{
		return true;
	}

	const bool bNeedUdp = (Protocol == ENetProtocol::UDP || Protocol == ENetProtocol::Both);
	const bool bNeedTcp = (Protocol == ENetProtocol::TCP || Protocol == ENetProtocol::Both);

	if (bNeedUdp && (ListenPort <= 0 || ListenPort > 65535))
	{
		UE_LOG(LogNetLan, Error, TEXT("[NetLan] 监听端口非法: %d"), ListenPort);
		return false;
	}

	bStarted = true;
	SetComponentTickEnabled(true);

	if (bNeedUdp)
	{
		StartUdpListening();
	}

	if (bNeedTcp)
	{
		if (TcpMode == ETcpMode::Server)
		{
			if (StartTcpListening())
			{
				BoundPort = ListenPort;
			}
		}
		else
		{
			// 客户端模式：先尝试连一次，失败交给 Tick 里的重连倒计时
			ReconnectCountdown = 0.0f;
			ConnectToServer();
		}
	}

	UE_LOG(LogNetLan, Log,
		TEXT("[NetLan] 已启动  协议=%s  角色=%s  分帧=%s  字节序=%s  %s"),
		*GetProtocolName(),
		TcpMode == ETcpMode::Client ? TEXT("客户端") : TEXT("服务器"),
		TcpFraming == ETcpFraming::Delimiter ? TEXT("分隔符") : TEXT("长度前缀"),
		LengthByteOrder == ELengthByteOrder::LittleEndian ? TEXT("LE") : TEXT("BE"),
		TcpMode == ETcpMode::Client
			? *FString::Printf(TEXT("目标 %s"), *GetServerAddress())
			: *FString::Printf(TEXT("监听 0.0.0.0:%d"), ListenPort));

	return true;
}

void UNetLanReceiverComponent::StopListening()
{
	if (!bStarted)
	{
		return;
	}

	SetComponentTickEnabled(false);

	StopUdpListening();
	StopTcpListening();
	CloseClientSocket();

	BoundPort = 0;
	bStarted = false;

	UE_LOG(LogNetLan, Log, TEXT("[NetLan] 已停止"));
}

// ==================== UDP ====================

bool UNetLanReceiverComponent::StartUdpListening()
{
	if (UdpSocket)
	{
		return true;
	}

	UdpSocket = FUdpSocketBuilder(TEXT("QiYuanNetLAN_UDP"))
		.AsNonBlocking()
		.BoundToPort(ListenPort)
		.WithReceiveBufferSize(2 * 1024 * 1024)
		.Build();

	if (!UdpSocket)
	{
		UE_LOG(LogNetLan, Error, TEXT("[NetLan] UDP 监听失败 端口 %d（被占用/防火墙）"), ListenPort);
		return false;
	}

	UE_LOG(LogNetLan, Log, TEXT("[NetLan] UDP 已监听 端口 %d"), ListenPort);
	return true;
}

void UNetLanReceiverComponent::StopUdpListening()
{
	if (!UdpSocket)
	{
		return;
	}

	UdpSocket->Close();
	if (ISocketSubsystem* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
	{
		Subsystem->DestroySocket(UdpSocket);
	}
	UdpSocket = nullptr;
}

// ==================== TCP 服务器 ====================

bool UNetLanReceiverComponent::StartTcpListening()
{
	if (TcpListenSocket)
	{
		return true;
	}

	ISocketSubsystem* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!Subsystem)
	{
		return false;
	}

	TSharedRef<FInternetAddr> Addr = Subsystem->CreateInternetAddr();
	Addr->SetAnyAddress();
	Addr->SetPort(ListenPort);

	TcpListenSocket = Subsystem->CreateSocket(NAME_Stream, TEXT("QiYuanNetLAN_TCP_Listen"), true);
	if (!TcpListenSocket)
	{
		return false;
	}

	TcpListenSocket->SetNonBlocking(true);
	TcpListenSocket->SetReuseAddr(true);
	TcpListenSocket->Bind(*Addr);

	if (!TcpListenSocket->Listen(16))
	{
		UE_LOG(LogNetLan, Error, TEXT("[NetLan] TCP Listen 失败 端口 %d"), ListenPort);
		Subsystem->DestroySocket(TcpListenSocket);
		TcpListenSocket = nullptr;
		return false;
	}

	UE_LOG(LogNetLan, Log, TEXT("[NetLan] TCP 已监听 端口 %d 等待连接..."), ListenPort);
	return true;
}

void UNetLanReceiverComponent::StopTcpListening()
{
	ISocketSubsystem* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	if (TcpListenSocket)
	{
		TcpListenSocket->Close();
		if (Subsystem)
		{
			Subsystem->DestroySocket(TcpListenSocket);
		}
		TcpListenSocket = nullptr;
	}

	for (FTcpClient* Client : TcpClients)
	{
		if (Client && Client->Socket)
		{
			Client->Socket->Close();
			if (Subsystem)
			{
				Subsystem->DestroySocket(Client->Socket);
			}
		}
		delete Client;
	}
	TcpClients.Reset();
}

// ==================== TCP 客户端（连二维端）====================

bool UNetLanReceiverComponent::ConnectToServer()
{
	if (ClientSocket)
	{
		return true;
	}

	ISocketSubsystem* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!Subsystem)
	{
		return false;
	}

	TSharedRef<FInternetAddr> Addr = Subsystem->CreateInternetAddr();
	bool bValidIp = false;
	Addr->SetIp(*ServerIP, bValidIp);
	if (!bValidIp)
	{
		UE_LOG(LogNetLan, Error, TEXT("[NetLan] 服务器 IP 非法: %s"), *ServerIP);
		return false;
	}
	Addr->SetPort(ServerPort);

	FSocket* NewSocket = Subsystem->CreateSocket(NAME_Stream, TEXT("QiYuanTelemetry_TCP_Client"), true);
	if (!NewSocket)
	{
		UE_LOG(LogNetLan, Error, TEXT("[NetLan] 创建 TCP 客户端 socket 失败"));
		return false;
	}

	NewSocket->SetNoDelay(true);

	// 阻塞连接（局域网内瞬时完成），连完立刻切回非阻塞轮询收包
	NewSocket->SetNonBlocking(false);
	const bool bConnected = NewSocket->Connect(*Addr);
	NewSocket->SetNonBlocking(true);

	if (!bConnected)
	{
		Subsystem->DestroySocket(NewSocket);
		UE_LOG(LogNetLan, Warning,
			TEXT("[NetLan] 连接二维端失败 %s，%.1f 秒后重试"),
			*GetServerAddress(), ReconnectInterval);
		ReconnectCountdown = ReconnectInterval;
		return false;
	}

	ClientSocket = NewSocket;
	ClientRecvBuffer.Reset();
	bHasHello = false;
	ReconnectCountdown = 0.0f;

	LastSenderIP = ServerIP;
	LastSenderPort = ServerPort;
	bHasLastSender = true;

	UE_LOG(LogNetLan, Log, TEXT("[NetLan] 已连接二维端 %s，等待 hello/tick/unit 数据..."), *GetServerAddress());
	OnConnectedChanged.Broadcast(true);
	return true;
}

void UNetLanReceiverComponent::CloseClientSocket()
{
	if (!ClientSocket)
	{
		return;
	}

	ClientSocket->Close();
	if (ISocketSubsystem* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
	{
		Subsystem->DestroySocket(ClientSocket);
	}
	ClientSocket = nullptr;

	ClientRecvBuffer.Reset();
	OnConnectedChanged.Broadcast(false);
}

void UNetLanReceiverComponent::DisconnectFromServer()
{
	if (ClientSocket)
	{
		UE_LOG(LogNetLan, Log, TEXT("[NetLan] 已断开与二维端的连接 %s"), *GetServerAddress());
	}
	CloseClientSocket();
}

// ==================== Tick ====================

void UNetLanReceiverComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	PollAll(DeltaTime);
}

void UNetLanReceiverComponent::PollAll(float DeltaTime)
{
	PollUdp();
	PollTcpAccept();
	PollTcpServerClients();
	PollTcpClient(DeltaTime);
	CleanupDisconnectedTcpClients();
}

void UNetLanReceiverComponent::PollUdp()
{
	if (!UdpSocket)
	{
		return;
	}

	ISocketSubsystem* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!Subsystem)
	{
		return;
	}

	uint32 PendingSize = 0;
	int32 Processed = 0;

	while (Processed < MaxPacketsPerTick && UdpSocket->HasPendingData(PendingSize))
	{
		++Processed;

		TArray<uint8> Buffer;
		Buffer.SetNumUninitialized(FMath::Min<int32>(static_cast<int32>(PendingSize), UdpMaxDatagramSize));

		TSharedRef<FInternetAddr> SenderAddr = Subsystem->CreateInternetAddr();
		int32 BytesRead = 0;

		if (!UdpSocket->RecvFrom(Buffer.GetData(), Buffer.Num(), BytesRead, *SenderAddr) || BytesRead <= 0)
		{
			break;
		}
		Buffer.SetNum(BytesRead);

		uint32 SenderIpValue = 0;
		SenderAddr->GetIp(SenderIpValue);
		const FIPv4Address SenderIPv4(SenderIpValue);

		LastSenderIP = SenderIPv4.ToString();
		LastSenderPort = SenderAddr->GetPort();
		bHasLastSender = true;

		DispatchMessage(Buffer, LastSenderIP, LastSenderPort);
	}
}

void UNetLanReceiverComponent::PollTcpAccept()
{
	if (!TcpListenSocket)
	{
		return;
	}

	ISocketSubsystem* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!Subsystem)
	{
		return;
	}

	TSharedRef<FInternetAddr> ClientAddr = Subsystem->CreateInternetAddr();

	while (true)
	{
		FSocket* NewSocket = TcpListenSocket->Accept(*ClientAddr, TEXT("QiYuanNetLAN_TCP_Client"));
		if (!NewSocket)
		{
			break;
		}

		NewSocket->SetNonBlocking(true);

		uint32 IpValue = 0;
		ClientAddr->GetIp(IpValue);
		const FString IpStr = FIPv4Address(IpValue).ToString();
		const int32 Port = ClientAddr->GetPort();

		FTcpClient* Client = new FTcpClient();
		Client->Socket = NewSocket;
		Client->SenderIP = IpStr;
		Client->SenderPort = Port;
		TcpClients.Add(Client);

		UE_LOG(LogNetLan, Log, TEXT("[NetLan] TCP 新连接 %s:%d (当前连接数=%d)"),
			*IpStr, Port, TcpClients.Num());
	}
}

void UNetLanReceiverComponent::PollTcpServerClients()
{
	if (TcpClients.Num() == 0)
	{
		return;
	}

	ISocketSubsystem* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	for (FTcpClient* Client : TcpClients)
	{
		if (!Client || Client->bDisconnected || !Client->Socket)
		{
			continue;
		}

		TArray<uint8> Chunk;
		Chunk.SetNumUninitialized(TcpRecvChunkSize);
		int32 BytesRead = 0;
		const bool bOk = Client->Socket->Recv(Chunk.GetData(), Chunk.Num(), BytesRead);

		if (!bOk)
		{
			// 非阻塞 socket 暂时无数据也会返回 false，用错误码区分
			const ESocketErrors LastError = Subsystem ? Subsystem->GetLastErrorCode() : SE_NO_ERROR;
			if (LastError == SE_EWOULDBLOCK)
			{
				continue;
			}
			Client->bDisconnected = true;
			UE_LOG(LogNetLan, Log, TEXT("[NetLan] TCP 断开 %s:%d"), *Client->SenderIP, Client->SenderPort);
			continue;
		}

		if (BytesRead <= 0)
		{
			continue;
		}

		Chunk.SetNum(BytesRead);
		Client->RecvBuffer.Append(Chunk);

		bool bShouldClose = false;
		ProcessReceiveBuffer(Client->RecvBuffer, Client->SenderIP, Client->SenderPort, bShouldClose);
		if (bShouldClose)
		{
			Client->bDisconnected = true;
		}
	}
}

void UNetLanReceiverComponent::PollTcpClient(float DeltaTime)
{
	if (TcpMode != ETcpMode::Client || Protocol == ENetProtocol::UDP)
	{
		return;
	}

	// 未连接：按间隔重连
	if (!ClientSocket)
	{
		if (bAutoReconnect && bStarted)
		{
			ReconnectCountdown -= DeltaTime;
			if (ReconnectCountdown <= 0.0f)
			{
				ConnectToServer();
			}
		}
		return;
	}

	TArray<uint8> Chunk;
	Chunk.SetNumUninitialized(TcpRecvChunkSize);
	int32 BytesRead = 0;
	const bool bOk = ClientSocket->Recv(Chunk.GetData(), Chunk.Num(), BytesRead);

	if (!bOk)
	{
		ISocketSubsystem* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		const ESocketErrors LastError = Subsystem ? Subsystem->GetLastErrorCode() : SE_NO_ERROR;
		if (LastError == SE_EWOULDBLOCK)
		{
			return; // 暂时无数据
		}

		UE_LOG(LogNetLan, Warning, TEXT("[NetLan] 与二维端断开 %s，%.1f 秒后重连"),
			*GetServerAddress(), ReconnectInterval);
		CloseClientSocket();
		ReconnectCountdown = ReconnectInterval;
		return;
	}

	if (BytesRead <= 0)
	{
		return; // 对端未发数据（或优雅关闭，由下一次 Recv 返回 false 处理）
	}

	Chunk.SetNum(BytesRead);
	ClientRecvBuffer.Append(Chunk);

	bool bShouldClose = false;
	ProcessReceiveBuffer(ClientRecvBuffer, ServerIP, ServerPort, bShouldClose);
	if (bShouldClose)
	{
		UE_LOG(LogNetLan, Warning, TEXT("[NetLan] 帧格式异常，断开 %s 并准备重连"), *GetServerAddress());
		CloseClientSocket();
		ReconnectCountdown = ReconnectInterval;
	}
}

void UNetLanReceiverComponent::CleanupDisconnectedTcpClients()
{
	ISocketSubsystem* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	for (int32 i = TcpClients.Num() - 1; i >= 0; --i)
	{
		FTcpClient* Client = TcpClients[i];
		if (!Client || Client->bDisconnected || !Client->Socket)
		{
			if (Client)
			{
				if (Client->Socket)
				{
					Client->Socket->Close();
					if (Subsystem)
					{
						Subsystem->DestroySocket(Client->Socket);
					}
				}
				delete Client;
			}
			TcpClients.RemoveAtSwap(i);
		}
	}
}

// ==================== 分帧 ====================

int32 UNetLanReceiverComponent::ProcessReceiveBuffer(TArray<uint8>& Buffer, const FString& SenderIP, int32 SenderPort, bool& bOutShouldClose)
{
	bOutShouldClose = false;

	if (Buffer.Num() == 0)
	{
		return 0;
	}

	int32 Offset = 0;
	int32 Processed = 0;

	while (Offset < Buffer.Num() && Processed < MaxPacketsPerTick)
	{
		TArray<uint8> Message;
		bool bGotMessage = false;
		bool bCounted = false;

		if (TcpFraming == ETcpFraming::Delimiter)
		{
			const FString DelimStr = Delimiter.IsEmpty() ? TEXT("\n") : Delimiter;
			const FTCHARToUTF8 DelimUtf8(*DelimStr);
			const int32 DelimLen = DelimUtf8.Length();
			const uint8* DelimBytes = reinterpret_cast<const uint8*>(DelimUtf8.Get());

			if (DelimLen <= 0)
			{
				break;
			}

			int32 FoundIdx = INDEX_NONE;
			for (int32 i = Offset; i + DelimLen <= Buffer.Num(); ++i)
			{
				bool bMatch = true;
				for (int32 j = 0; j < DelimLen; ++j)
				{
					if (Buffer[i + j] != DelimBytes[j])
					{
						bMatch = false;
						break;
					}
				}
				if (bMatch)
				{
					FoundIdx = i;
					break;
				}
			}

			if (FoundIdx == INDEX_NONE)
			{
				if (Buffer.Num() - Offset > MaxFrameSize)
				{
					UE_LOG(LogNetLan, Warning,
						TEXT("[NetLan] 缓冲超过 %d 字节仍无分隔符，断开 %s:%d"),
						MaxFrameSize, *SenderIP, SenderPort);
					bOutShouldClose = true;
				}
				break;
			}

			const int32 MsgLen = FoundIdx - Offset;
			if (MsgLen > 0)
			{
				Message.Append(Buffer.GetData() + Offset, MsgLen);
				bGotMessage = true;
			}
			Offset = FoundIdx + DelimLen;
			bCounted = true;
		}
		else // LengthPrefix
		{
			if (Buffer.Num() - Offset < 4)
			{
				break;
			}

			const uint8* P = Buffer.GetData() + Offset;
			uint32 MsgLen = 0;
			if (LengthByteOrder == ELengthByteOrder::LittleEndian)
			{
				MsgLen = uint32(P[0]) | (uint32(P[1]) << 8) | (uint32(P[2]) << 16) | (uint32(P[3]) << 24);
			}
			else
			{
				MsgLen = (uint32(P[0]) << 24) | (uint32(P[1]) << 16) | (uint32(P[2]) << 8) | uint32(P[3]);
			}

			if (MsgLen > uint32(MaxFrameSize))
			{
				UE_LOG(LogNetLan, Warning,
					TEXT("[NetLan] 帧长度 %u 超过上限 %d，断开 %s:%d"),
					MsgLen, MaxFrameSize, *SenderIP, SenderPort);
				bOutShouldClose = true;
				break;
			}

			const int32 Total = 4 + static_cast<int32>(MsgLen);
			if (Buffer.Num() - Offset < Total)
			{
				break; // 半包，等下一批
			}

			if (MsgLen > 0)
			{
				Message.Append(P + 4, static_cast<int32>(MsgLen));
				bGotMessage = true;
			}
			Offset += Total;
			bCounted = true;
		}

		if (bCounted)
		{
			++Processed;
		}

		if (bGotMessage)
		{
			LastSenderIP = SenderIP;
			LastSenderPort = SenderPort;
			bHasLastSender = true;

			DispatchMessage(Message, SenderIP, SenderPort);
		}

		if (bOutShouldClose)
		{
			break;
		}
	}

	if (Offset >= Buffer.Num())
	{
		Buffer.Reset();
	}
	else if (Offset > 0)
	{
		TArray<uint8> Remaining;
		Remaining.Append(Buffer.GetData() + Offset, Buffer.Num() - Offset);
		Buffer = MoveTemp(Remaining);
	}

	return Processed;
}

// ==================== 消息分发 ====================

void UNetLanReceiverComponent::DispatchMessage(const TArray<uint8>& Data, const FString& SenderIP, int32 SenderPort)
{
	OnBytesReceived.Broadcast(Data, SenderIP, SenderPort);
	TrimAndDispatchText(Data, SenderIP, SenderPort);
}

void UNetLanReceiverComponent::TrimAndDispatchText(const TArray<uint8>& Data, const FString& SenderIP, int32 SenderPort)
{
	int32 Len = Data.Num();
	while (Len > 0)
	{
		const uint8 Tail = Data[Len - 1];
		if (Tail == 0 || Tail == '\n' || Tail == '\r')
		{
			--Len;
		}
		else
		{
			break;
		}
	}

	if (Len <= 0)
	{
		return;
	}

	FUTF8ToTCHAR Conv(reinterpret_cast<const ANSICHAR*>(Data.GetData()), Len);
	const FString Message(Conv.Length(), Conv.Get());

	// JSON 消息按类型解析（unit 的日志由 HandleJsonMessage 控制）
	if (Message.StartsWith(TEXT("{")))
	{
		HandleJsonMessage(Message, SenderIP, SenderPort);
	}
	else if (bLogMessages)
	{
		UE_LOG(LogNetLan, Log, TEXT("[NetLan] 收到 %d 字节 来自 %s:%d -> %s"),
			Data.Num(), *SenderIP, SenderPort, *Message.Left(512));
	}

	OnMessageReceived.Broadcast(Message, SenderIP, SenderPort);
}

// ==================== JSON 解析 ====================

void UNetLanReceiverComponent::HandleJsonMessage(const FString& Json, const FString& SenderIP, int32 SenderPort)
{
	TSharedPtr<FJsonObject> Obj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid())
	{
		if (bLogMessages)
		{
			UE_LOG(LogNetLan, Warning, TEXT("[NetLan] JSON 解析失败: %s"), *Json.Left(200));
		}
		return;
	}

	const FString Type = JsonString(Obj, TEXT("m"));

	// x_speed 在 hello / tick / unit / control_speed 中都可能出现
	if (Obj->HasField(TEXT("x_speed")))
	{
		const int32 Speed = JsonInt(Obj, TEXT("x_speed"), LastSpeed);
		if (!bSpeedKnown || Speed != LastSpeed)
		{
			LastSpeed = Speed;
			bSpeedKnown = true;
			OnSpeedChanged.Broadcast(Speed);
		}
	}

	if (Type == TEXT("hello"))
	{
		FTelemetryHello Hello;
		Hello.Version = JsonInt(Obj, TEXT("v"), 1);
		Hello.Server = JsonString(Obj, TEXT("server"));
		Hello.Frame = JsonString(Obj, TEXT("frame"));
		Hello.Coord = JsonString(Obj, TEXT("coord"));
		Hello.Axes = JsonString(Obj, TEXT("axes"));
		Hello.LengthUnit = JsonString(Obj, TEXT("length_unit"));
		Hello.Tick = JsonInt(Obj, TEXT("tick"), 0);
		Hello.XSpeed = JsonInt(Obj, TEXT("x_speed"), LastSpeed);
		Hello.DetailIntervalMs = JsonInt(Obj, TEXT("detail_interval_ms"), 1000);
		Hello.SimStart = JsonString(Obj, TEXT("sim_start"));
		Hello.Timestamp = JsonNumber(Obj, TEXT("ts"), 0.0);

		const TSharedPtr<FJsonObject>* OriginPtr = nullptr;
		if (Obj->TryGetObjectField(TEXT("origin"), OriginPtr) && OriginPtr && OriginPtr->IsValid())
		{
			Hello.Origin.Lon = JsonNumber(*OriginPtr, TEXT("lon"), 0.0);
			Hello.Origin.Lat = JsonNumber(*OriginPtr, TEXT("lat"), 0.0);
			Hello.Origin.Alt = JsonNumber(*OriginPtr, TEXT("alt"), 0.0);
			Hello.bHasOrigin = true;
		}

		CachedHello = Hello;
		bHasHello = true;

		UE_LOG(LogNetLan, Log,
			TEXT("[NetLan] hello: server=%s coord=%s axes=%s unit=%s origin=(%.7f, %.7f, %.1f) tick=%d"),
			*Hello.Server, *Hello.Coord, *Hello.Axes, *Hello.LengthUnit,
			Hello.Origin.Lon, Hello.Origin.Lat, Hello.Origin.Alt, Hello.Tick);

		OnHelloReceived.Broadcast(Hello);
	}
	else if (Type == TEXT("tick"))
	{
		FTelemetryTick TickInfo;
		TickInfo.Tick = JsonInt(Obj, TEXT("tick"), 0);
		TickInfo.XSpeed = JsonInt(Obj, TEXT("x_speed"), LastSpeed);
		TickInfo.SimStart = JsonString(Obj, TEXT("sim_start"));
		TickInfo.Timestamp = JsonNumber(Obj, TEXT("ts"), 0.0);

		if (bLogMessages)
		{
			UE_LOG(LogNetLan, Verbose, TEXT("[NetLan] tick=%d speed=%d"), TickInfo.Tick, TickInfo.XSpeed);
		}

		OnTickReceived.Broadcast(TickInfo);
	}
	else if (Type == TEXT("unit"))
	{
		FTelemetryUnit Unit;
		Unit.Id = JsonString(Obj, TEXT("id"));
		Unit.Name = JsonString(Obj, TEXT("name"));
		Unit.Party = JsonInt(Obj, TEXT("party"), -1);
		Unit.Army = JsonString(Obj, TEXT("army"));
		Unit.ArmyName = JsonString(Obj, TEXT("army_name"));
		Unit.Side = JsonInt(Obj, TEXT("side"), 3);
		Unit.Diplomacy = JsonString(Obj, TEXT("diplomacy"));
		Unit.Kind = JsonString(Obj, TEXT("kind"));
		Unit.ObjType = JsonInt(Obj, TEXT("obj_type"), 0);
		Unit.Model3D = JsonString(Obj, TEXT("3d_model"));
		Unit.Category3D = JsonString(Obj, TEXT("3d_category"));
		Unit.ArmyModel = JsonString(Obj, TEXT("army_model"));
		Unit.ArmyCategory = JsonString(Obj, TEXT("army_category"));
		Unit.Tick = JsonInt(Obj, TEXT("tick"), 0);
		Unit.XSpeed = JsonInt(Obj, TEXT("x_speed"), LastSpeed);
		Unit.Timestamp = JsonNumber(Obj, TEXT("ts"), 0.0);
		Unit.bHasDetail = JsonInt(Obj, TEXT("detail"), 0) != 0;

		if (Obj->HasField(TEXT("lon")) && Obj->HasField(TEXT("lat")))
		{
			Unit.Lon = JsonNumber(Obj, TEXT("lon"), 0.0);
			Unit.Lat = JsonNumber(Obj, TEXT("lat"), 0.0);
			Unit.bHasLonLat = true;
		}
		Unit.Alt = JsonNumber(Obj, TEXT("alt"), 0.0);

		if (Obj->HasField(TEXT("x")) && Obj->HasField(TEXT("y")))
		{
			Unit.X = JsonNumber(Obj, TEXT("x"), 0.0);
			Unit.Y = JsonNumber(Obj, TEXT("y"), 0.0);
			Unit.Z = JsonNumber(Obj, TEXT("z"), 0.0);
			Unit.bHasXYZ = true;
		}

		Unit.Heading = JsonNumber(Obj, TEXT("heading"), 0.0);
		Unit.Speed = JsonNumber(Obj, TEXT("speed"), 0.0);
		Unit.Height = JsonNumber(Obj, TEXT("height"), 0.0);
		Unit.bAlive = JsonBool(Obj, TEXT("alive"), true);
		Unit.bPrisoner = JsonBool(Obj, TEXT("prisoner"), false);
		Unit.bLeader = JsonBool(Obj, TEXT("leader"), false);
		Unit.State = JsonInt(Obj, TEXT("state"), 0);
		Unit.Life = JsonInt(Obj, TEXT("life"), 100);
		Unit.RawJson = Json;

		if (!Unit.Id.IsEmpty())
		{
			UnitMap.Add(Unit.Id, Unit);
		}

		if (bLogMessages && bLogUnitMessages)
		{
			UE_LOG(LogNetLan, Log,
				TEXT("[NetLan] unit %s(%s) pos=(%.7f, %.7f) alt=%.1f enu=(%.1f, %.1f, %.1f) heading=%.1f life=%d"),
				*Unit.Id, *Unit.Name, Unit.Lon, Unit.Lat, Unit.Alt,
				Unit.X, Unit.Y, Unit.Z, Unit.Heading, Unit.Life);
		}

		OnUnitReceived.Broadcast(Unit);
	}
	else if (Type == TEXT("control_speed"))
	{
		// x_speed 已在上方统一处理
	}
	else if (Type == TEXT("control_state"))
	{
		const int32 State = JsonInt(Obj, TEXT("state"), 0);
		UE_LOG(LogNetLan, Log, TEXT("[NetLan] 推演状态 -> %s"), State == 0 ? TEXT("运行") : TEXT("暂停"));
		OnSimStateChanged.Broadcast(State);
	}
	else if (Type == TEXT("control_focus"))
	{
		const FString FocusId = JsonString(Obj, TEXT("id"));
		const FString FocusName = JsonString(Obj, TEXT("name"));
		UE_LOG(LogNetLan, Log, TEXT("[NetLan] 聚焦 %s %s"), *FocusId, *FocusName);
		OnFocusChanged.Broadcast(FocusId, FocusName);
	}
	else if (bLogMessages)
	{
		UE_LOG(LogNetLan, Log, TEXT("[NetLan] %s: %s"), *Type, *Json.Left(512));
	}

	// 所有类型统一抛一次，方便蓝图自行取字段
	OnJsonMessage.Broadcast(Type, Json, SenderIP, SenderPort);
}

// ==================== 数据查询 ====================

bool UNetLanReceiverComponent::GetUnitById(const FString& Id, FTelemetryUnit& OutUnit) const
{
	if (const FTelemetryUnit* Found = UnitMap.Find(Id))
	{
		OutUnit = *Found;
		return true;
	}
	return false;
}

TArray<FTelemetryUnit> UNetLanReceiverComponent::GetAllUnits() const
{
	TArray<FTelemetryUnit> Result;
	Result.Reserve(UnitMap.Num());
	for (const TPair<FString, FTelemetryUnit>& Pair : UnitMap)
	{
		Result.Add(Pair.Value);
	}
	return Result;
}

void UNetLanReceiverComponent::ClearUnits()
{
	UnitMap.Reset();
}

FString UNetLanReceiverComponent::JsonGetString(const FString& Json, const FString& Key)
{
	TSharedPtr<FJsonObject> Obj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (FJsonSerializer::Deserialize(Reader, Obj) && Obj.IsValid())
	{
		return JsonString(Obj, *Key);
	}
	return FString();
}

float UNetLanReceiverComponent::JsonGetNumber(const FString& Json, const FString& Key)
{
	TSharedPtr<FJsonObject> Obj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (FJsonSerializer::Deserialize(Reader, Obj) && Obj.IsValid())
	{
		return static_cast<float>(JsonNumber(Obj, *Key, 0.0));
	}
	return 0.0f;
}

// ==================== 发送 ====================

void UNetLanReceiverComponent::ApplyFraming(const TArray<uint8>& Data, TArray<uint8>& OutBuffer) const
{
	OutBuffer.Reset();

	if (TcpFraming == ETcpFraming::Delimiter)
	{
		OutBuffer.Append(Data);
		const FString Delim = Delimiter.IsEmpty() ? TEXT("\n") : Delimiter;
		const FTCHARToUTF8 DelimUtf8(*Delim);
		OutBuffer.Append(reinterpret_cast<const uint8*>(DelimUtf8.Get()), DelimUtf8.Length());
		return;
	}

	const uint32 Length = static_cast<uint32>(Data.Num());
	uint8 Header[4];
	if (LengthByteOrder == ELengthByteOrder::LittleEndian)
	{
		Header[0] = uint8(Length & 0xFF);
		Header[1] = uint8((Length >> 8) & 0xFF);
		Header[2] = uint8((Length >> 16) & 0xFF);
		Header[3] = uint8((Length >> 24) & 0xFF);
	}
	else
	{
		Header[0] = uint8((Length >> 24) & 0xFF);
		Header[1] = uint8((Length >> 16) & 0xFF);
		Header[2] = uint8((Length >> 8) & 0xFF);
		Header[3] = uint8(Length & 0xFF);
	}
	OutBuffer.Append(Header, 4);
	OutBuffer.Append(Data);
}

bool UNetLanReceiverComponent::SendString(const FString& Message, const FString& TargetIP, int32 TargetPort)
{
	FTCHARToUTF8 Conv(*Message);
	const TArray<uint8> Data(reinterpret_cast<const uint8*>(Conv.Get()), Conv.Length());
	return SendBytes(Data, TargetIP, TargetPort);
}

bool UNetLanReceiverComponent::SendBytes(const TArray<uint8>& Data, const FString& TargetIP, int32 TargetPort)
{
	if (Data.Num() <= 0)
	{
		return false;
	}

	bool bOk = false;
	if (Protocol == ENetProtocol::UDP || Protocol == ENetProtocol::Both)
	{
		bOk |= SendBytesUdp(Data, TargetIP, TargetPort);
	}
	if (Protocol == ENetProtocol::TCP || Protocol == ENetProtocol::Both)
	{
		bOk |= SendBytesTcp(Data, TargetIP, TargetPort);
	}
	return bOk;
}

bool UNetLanReceiverComponent::SendBytesUdp(const TArray<uint8>& Data, const FString& TargetIP, int32 TargetPort)
{
	ISocketSubsystem* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!Subsystem)
	{
		return false;
	}

	FSocket* Sock = UdpSocket;
	bool bNeedDestroy = false;
	if (!Sock)
	{
		Sock = FUdpSocketBuilder(TEXT("QiYuanNetLAN_UDP_Send"))
			.AsNonBlocking()
			.WithSendBufferSize(1024 * 1024)
			.Build();
		if (!Sock)
		{
			return false;
		}
		bNeedDestroy = true;
	}

	TSharedRef<FInternetAddr> Addr = Subsystem->CreateInternetAddr();
	bool bValidIp = false;
	Addr->SetIp(*TargetIP, bValidIp);
	if (!bValidIp)
	{
		UE_LOG(LogNetLan, Error, TEXT("[NetLan] 目标 IP 非法: %s"), *TargetIP);
		if (bNeedDestroy)
		{
			Subsystem->DestroySocket(Sock);
		}
		return false;
	}
	Addr->SetPort(TargetPort);

	int32 BytesSent = 0;
	const bool bOk = Sock->SendTo(Data.GetData(), Data.Num(), BytesSent, *Addr) && BytesSent == Data.Num();
	if (!bOk)
	{
		UE_LOG(LogNetLan, Warning, TEXT("[NetLan] UDP 发送失败 -> %s:%d (已发 %d / %d)"),
			*TargetIP, TargetPort, BytesSent, Data.Num());
	}
	if (bNeedDestroy)
	{
		Subsystem->DestroySocket(Sock);
	}
	return bOk;
}

bool UNetLanReceiverComponent::SendBytesTcp(const TArray<uint8>& Data, const FString& TargetIP, int32 TargetPort)
{
	ISocketSubsystem* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!Subsystem)
	{
		return false;
	}

	FSocket* Sock = nullptr;

	if (TcpMode == ETcpMode::Client)
	{
		if (!ClientSocket)
		{
			UE_LOG(LogNetLan, Warning, TEXT("[NetLan] TCP 未连接二维端，无法发送"));
			return false;
		}
		Sock = ClientSocket;
	}
	else
	{
		FTcpClient* TargetClient = nullptr;
		for (FTcpClient* Client : TcpClients)
		{
			if (Client && !Client->bDisconnected && Client->SenderIP == TargetIP && Client->SenderPort == TargetPort)
			{
				TargetClient = Client;
				break;
			}
		}

		if (!TargetClient)
		{
			TSharedRef<FInternetAddr> Addr = Subsystem->CreateInternetAddr();
			bool bValidIp = false;
			Addr->SetIp(*TargetIP, bValidIp);
			if (!bValidIp)
			{
				return false;
			}
			Addr->SetPort(TargetPort);

			FSocket* NewSocket = Subsystem->CreateSocket(NAME_Stream, TEXT("QiYuanNetLAN_TCP_Out"), true);
			if (!NewSocket)
			{
				return false;
			}

			NewSocket->SetNonBlocking(false);
			const bool bConnected = NewSocket->Connect(*Addr);
			NewSocket->SetNonBlocking(true);

			if (!bConnected)
			{
				UE_LOG(LogNetLan, Warning, TEXT("[NetLan] TCP Connect 失败 -> %s:%d"), *TargetIP, TargetPort);
				Subsystem->DestroySocket(NewSocket);
				return false;
			}

			FTcpClient* Client = new FTcpClient();
			Client->Socket = NewSocket;
			Client->SenderIP = TargetIP;
			Client->SenderPort = TargetPort;
			TcpClients.Add(Client);
			TargetClient = Client;
		}

		Sock = TargetClient->Socket;
	}

	TArray<uint8> ToSend;
	ApplyFraming(Data, ToSend);

	int32 BytesSent = 0;
	const bool bOk = Sock->Send(ToSend.GetData(), ToSend.Num(), BytesSent) && BytesSent == ToSend.Num();
	if (!bOk)
	{
		UE_LOG(LogNetLan, Warning, TEXT("[NetLan] TCP 发送失败 -> %s:%d (已发 %d / %d)"),
			*TargetIP, TargetPort, BytesSent, ToSend.Num());
	}
	return bOk;
}

bool UNetLanReceiverComponent::SendStringToLastSender(const FString& Message)
{
	if (!bHasLastSender)
	{
		UE_LOG(LogNetLan, Warning, TEXT("[NetLan] 还没收到过数据，无法回复"));
		return false;
	}
	return SendString(Message, LastSenderIP, LastSenderPort);
}

bool UNetLanReceiverComponent::SendBytesToLastSender(const TArray<uint8>& Data)
{
	if (!bHasLastSender)
	{
		UE_LOG(LogNetLan, Warning, TEXT("[NetLan] 还没收到过数据，无法回复"));
		return false;
	}
	return SendBytes(Data, LastSenderIP, LastSenderPort);
}
