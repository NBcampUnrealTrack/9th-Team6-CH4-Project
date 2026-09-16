#include "AudioSettingSubsystem.h"
#include "AudioCaptureCore.h"
#include "AudioCaptureBlueprintLibrary.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Async/Async.h"
#include "UObject/Stack.h"

static void execSafeGetAvailableAudioInputDevices(UObject* Context, FFrame& Stack, RESULT_DECL)
{
	P_GET_OBJECT(const UObject, WorldContextObject);
	FOnAudioInputDevicesObtained OnObtainDevicesEvent;
	Stack.StepCompiledIn<FDelegateProperty>(&OnObtainDevicesEvent);
	P_FINISH;
	P_NATIVE_BEGIN;

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [OnObtainDevicesEvent]()
	{
		Audio::FAudioCapture AudioCapture;
		TArray<Audio::FCaptureDeviceInfo> InputDevices;
		AudioCapture.GetCaptureDevicesAvailable(InputDevices);

		TArray<FAudioInputDeviceInfo> AvailableDeviceInfos;
		for (const Audio::FCaptureDeviceInfo& Device : InputDevices)
		{
			AvailableDeviceInfos.Add(FAudioInputDeviceInfo(Device));
		}

		AsyncTask(ENamedThreads::GameThread, [OnObtainDevicesEvent, AvailableDeviceInfos = MoveTemp(AvailableDeviceInfos)]()
		{
			OnObtainDevicesEvent.ExecuteIfBound(AvailableDeviceInfos);
		});
	});
	P_NATIVE_END;
}

static void HookAudioCaptureFunction()
{
	static bool bHooked = false;
	if (!bHooked)
	{
		if (UClass* LibClass = UAudioCaptureBlueprintLibrary::StaticClass())
		{
			if (UFunction* Func = LibClass->FindFunctionByName(FName(TEXT("GetAvailableAudioInputDevices"))))
			{
				Func->SetNativeFunc(&execSafeGetAvailableAudioInputDevices);
				bHooked = true;
				UE_LOG(LogTemp, Log, TEXT("[AudioSettingSubsystem] Successfully hooked UAudioCaptureBlueprintLibrary::GetAvailableAudioInputDevices with GameThread dispatcher."));
			}
		}
	}
}

void UAudioSettingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	HookAudioCaptureFunction();

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
	auto UpdateFunc = [this, AvailableDevices]()
	{
		CachedOutputDevices = AvailableDevices;
		CachedOutputDeviceNames.Empty();

		for (const FAudioOutputDeviceInfo& Device : AvailableDevices)
		{
			CachedOutputDeviceNames.Add(Device.Name);
		}
	};

	if (IsInGameThread())
	{
		UpdateFunc();
	}
	else
	{
		AsyncTask(ENamedThreads::GameThread, UpdateFunc);
	}
}

void UAudioSettingSubsystem::FetchAudioInputDevices()
{
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this]()
	{
		Audio::FAudioCapture AudioCapture;
		TArray<Audio::FCaptureDeviceInfo> InputDevices;
		AudioCapture.GetCaptureDevicesAvailable(InputDevices);

		AsyncTask(ENamedThreads::GameThread, [this, InputDevices = MoveTemp(InputDevices)]()
		{
			CachedInputDeviceNames.Empty();
			for (const Audio::FCaptureDeviceInfo& Device : InputDevices)
			{
				CachedInputDeviceNames.Add(Device.DeviceName);
			}
		});
	});
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

UAudioSettingSubsystem::UAudioSettingSubsystem()
{
	static ConstructorHelpers::FObjectFinder<USoundMix> MasterMixAsset(TEXT("/Game/Audio/Master_Mix.Master_Mix"));
	if (MasterMixAsset.Succeeded())
	{
		MasterSoundMix = MasterMixAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<USoundClass> MasterSCAsset(TEXT("/Game/Audio/Master_SC.Master_SC"));
	if (MasterSCAsset.Succeeded())
	{
		MasterSoundClass = MasterSCAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<USoundClass> BGM_SCAsset(TEXT("/Game/Audio/BGM_SC.BGM_SC"));
	if (BGM_SCAsset.Succeeded())
	{
		BGMSoundClass = BGM_SCAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<USoundClass> SFX_SCAsset(TEXT("/Game/Audio/SFX_SC.SFX_SC"));
	if (SFX_SCAsset.Succeeded())
	{
		SFXSoundClass = SFX_SCAsset.Object;
	}
}