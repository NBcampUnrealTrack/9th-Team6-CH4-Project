#pragma once

#include "CoreMinimal.h"
#include "AudioDevice.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AudioSettingSubsystem.generated.h"


UCLASS()
class OPERATOR_API UAudioSettingSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	
	// 사용 가능한 오디오 출력 장치 목록 가져오기
	UFUNCTION(BlueprintCallable, Category = "Audio|Device")
	void FetchAudioOutputDevices();

	// 블루프린트에서 목록을 읽어갈 수 있도록 반환
	UFUNCTION(BlueprintCallable, Category = "Audio|Device")
	TArray<FString> GetAudioOutputDevices() const { return CachedOutputDeviceNames; }

	// 선택한 오디오 출력 장치로 변경
	UFUNCTION(BlueprintCallable, Category = "Audio|Device")
	void SetAudioOutputDevice(FString DeviceIdOrName);

	// 사용 가능한 오디오 입력(마이크) 장치 목록 가져오기
	UFUNCTION(BlueprintCallable, Category = "Audio|Device")
	void FetchAudioInputDevices();

	// 블루프린트에서 입력 장치 목록 반환
	UFUNCTION(BlueprintCallable, Category = "Audio|Device")
	TArray<FString> GetAudioInputDevices() const { return CachedInputDeviceNames; }
	
	// --- 에셋 할당 변수 (에디터/블루프린트 설정용) ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Settings")
	TObjectPtr<USoundMix> MasterSoundMix;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Settings")
	TObjectPtr<USoundClass> MasterSoundClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Settings")
	TObjectPtr<USoundClass> BGMSoundClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Settings")
	TObjectPtr<USoundClass> SFXSoundClass;

	// --- UI 연동용 볼륨 조절 함수 ---
	UFUNCTION(BlueprintCallable, Category = "Audio|Control")
	void SetMasterVolume(float Volume);

	UFUNCTION(BlueprintCallable, Category = "Audio|Control")
	void SetBGMVolume(float Volume);

	UFUNCTION(BlueprintCallable, Category = "Audio|Control")
	void SetSFXVolume(float Volume);
	
	// UI 값 불러오기용 Getter 함수 (매개변수 없음)
	UFUNCTION(BlueprintPure, Category = "Audio")
	float GetMasterVolume() const { return MasterVolume; }

	UFUNCTION(BlueprintPure, Category = "Audio")
	float GetBGMVolume() const { return BGMVolume; }

	UFUNCTION(BlueprintPure, Category = "Audio")
	float GetSFXVolume() const { return SFXVolume; }
	
	void InitAudioSettings();
	
protected:
	// 장치 목록 수집 완료 콜백
	UFUNCTION()
	void OnAudioOutputDevicesObtained(const TArray<FAudioOutputDeviceInfo>& AvailableDevices);

	// 장치 전환 완료 콜백
	UFUNCTION()
	void OnAudioDeviceSwapped(const FSwapAudioOutputResult& SwapResult);

private:
	void ApplyVolume(USoundClass* TargetClass, float Volume);
	
	TArray<FString> CachedOutputDeviceNames;
	TArray<FAudioOutputDeviceInfo> CachedOutputDevices;

	TArray<FString> CachedInputDeviceNames;
	
	float MasterVolume = 1.0f;
	float BGMVolume = 1.0f;
	float SFXVolume = 1.0f;
};