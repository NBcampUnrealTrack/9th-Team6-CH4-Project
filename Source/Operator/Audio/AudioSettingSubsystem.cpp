#include "AudioSettingSubsystem.h"
#include "AudioCaptureCore.h"
#include "AudioDeviceManager.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

void UAudioSettingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 에셋 경로를 C++에서 직접 로드 (블루프린트 할당 누락 방지)
	MasterSoundMix = LoadObject<USoundMix>(nullptr, TEXT("/Game/Audio/Master_Mix.Master_Mix"));
	MasterSoundClass = LoadObject<USoundClass>(nullptr, TEXT("/Game/Audio/Master_SC.Master_SC"));
	BGMSoundClass = LoadObject<USoundClass>(nullptr, TEXT("/Game/Audio/BGM_SC.BGM_SC"));
	SFXSoundClass = LoadObject<USoundClass>(nullptr, TEXT("/Game/Audio/SFX_SC.SFX_SC"));
}

void UAudioSettingSubsystem::ApplyVolume(USoundClass* TargetClass, float Volume)
{
	UWorld* World = GetWorld();
	if (!World || !MasterSoundMix || !TargetClass)
	{
		return;
	}

	// 볼륨 범위 제한 (0.0 ~ 1.0)
	const float ClampedVolume = FMath::Clamp(Volume, 0.0f, 1.0f);

	// Sound Mix에 Sound Class의 볼륨 오버라이드 적용
	UGameplayStatics::SetSoundMixClassOverride
	(
		World,
		MasterSoundMix,
		TargetClass,
		ClampedVolume,
		1.0f, // Pitch
		0.0f, // FadeIn Time
		true  // Apply to Children
	);

	// Sound Mix 활성화
	UGameplayStatics::PushSoundMixModifier(World, MasterSoundMix);
}

void UAudioSettingSubsystem::SetMasterVolume(float Volume)
{
	MasterVolume = Volume;
	ApplyVolume(MasterSoundClass, Volume);
}

void UAudioSettingSubsystem::SetBGMVolume(float Volume)
{
	BGMVolume = Volume;
	ApplyVolume(BGMSoundClass, Volume);
}

void UAudioSettingSubsystem::SetSFXVolume(float Volume)
{
	SFXVolume = Volume;
	ApplyVolume(SFXSoundClass, Volume);
}

// AudioSettingSubsystem.cpp
void UAudioSettingSubsystem::InitAudioSettings()
{
	if (UWorld* World = GetWorld())
	{
		if (MasterSoundMix)
		{
			UGameplayStatics::PushSoundMixModifier(World, MasterSoundMix);
		}
	}
}

void UAudioSettingSubsystem::FetchAudioOutputDevices()
{
	if (!GetWorld()) return;

	FOnAudioOutputDevicesObtained OnObtained;
	OnObtained.BindDynamic(this, &UAudioSettingSubsystem::OnAudioOutputDevicesObtained);

	// 언리얼 Engine API: GetAvailableAudioOutputDevices
	UAudioMixerBlueprintLibrary::GetAvailableAudioOutputDevices(GetWorld(), OnObtained);
}

void UAudioSettingSubsystem::OnAudioOutputDevicesObtained(const TArray<FAudioOutputDeviceInfo>& AvailableDevices)
{
	CachedOutputDevices = AvailableDevices;
	CachedOutputDeviceNames.Empty();

	for (const FAudioOutputDeviceInfo& Device : AvailableDevices)
	{
		CachedOutputDeviceNames.Add(Device.Name);
	}
}

void UAudioSettingSubsystem::SetAudioOutputDevice(FString DeviceIdOrName)
{
	if (!GetWorld()) return;

	FOnCompletedDeviceSwap OnCompleted;
	OnCompleted.BindDynamic(this, &UAudioSettingSubsystem::OnAudioDeviceSwapped);

	// 디바이스 ID 조회 (이름으로 전달받았을 경우 ID 매핑)
	FString TargetDeviceId = DeviceIdOrName;
	for (const FAudioOutputDeviceInfo& Device : CachedOutputDevices)
	{
		if (Device.Name == DeviceIdOrName)
		{
			TargetDeviceId = Device.DeviceId;
			break;
		}
	}

	// 올바른 언리얼 API 함수: SwapAudioOutputDevice
	UAudioMixerBlueprintLibrary::SwapAudioOutputDevice(GetWorld(), TargetDeviceId, OnCompleted);
}

void UAudioSettingSubsystem::OnAudioDeviceSwapped(const FSwapAudioOutputResult& SwapResult)
{
	UE_LOG(LogTemp, Log, TEXT("[AudioSubsystem] Swapped to Device ID: %s"), *SwapResult.RequestedDeviceId);
}
