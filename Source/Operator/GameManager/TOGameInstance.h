#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Containers/Ticker.h"

class FOnlineSessionSearch;
class UUserWidget;
class UButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRoomsFound);

#include "TOGameInstance.generated.h"

UCLASS()
class OPERATOR_API UTOGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    UTOGameInstance();

    virtual void Init() override;
    virtual void Shutdown() override;

    UPROPERTY(BlueprintAssignable, Category = "Session")
    FOnRoomsFound OnRoomsFound;

    UFUNCTION(BlueprintCallable, Category = "Session")
    int32 GetFoundRoomCount() const;

    UFUNCTION(BlueprintCallable, Category = "Session")
    FString GetFoundRoomName(int32 Index) const;

    UFUNCTION(BlueprintCallable, Category = "Session")
    int32 GetFoundRoomPlayerCount(int32 Index) const;
    
    // 방 생성 (비밀번호 및 방 이름 지원)
    UFUNCTION(BlueprintCallable, Category = "Session")
    void CreateMySession(const FString& Password = TEXT(""), const FString& InRoomName = TEXT(""));

    UFUNCTION(BlueprintCallable, Category = "Session")
    void CreateRoom(const FString& Password = TEXT(""), const FString& InRoomName = TEXT(""));
    
    UFUNCTION(BlueprintCallable, Category = "Session")
    void JoinMySession();

    UFUNCTION(BlueprintCallable, Category = "Session")
    void JoinRoom();

    // 공개 방 검색
    UFUNCTION(BlueprintCallable, Category = "Session")
    void FindRooms();

    // 비공개 방 검색 (비밀번호 및 방 이름 지원)
    UFUNCTION(BlueprintCallable, Category = "Session")
    void FindPrivateRooms(const FString& SearchPassword, const FString& SearchRoomName = TEXT(""));
    
    UFUNCTION(BlueprintCallable, Category = "Session")
    void JoinFoundSession(int32 Index);

    // IP 직접 접속 (하마치 / LAN 테스트용)
    UFUNCTION(BlueprintCallable, Category = "Session")
    void JoinServerByIP(const FString& IPAddress);

    // WBP_JoinRoomMenu UI 버튼 자동 바인딩 핸들러
    UFUNCTION()
    void OnPrivateJoinButtonClicked();

    UFUNCTION()
    void OnRefreshServerClicked();

    UFUNCTION()
    void OnPublicSearchButtonClicked();

    FString GetBestHostIP() const;
    
    // =========================================================================
    // Customization & Session Info
    // =========================================================================
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator|PlayerInfo")
    FString PlayerName = TEXT("Player");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator|Session")
    FString RoomName = TEXT("");
    
    void SetPlayerName(FString& InMyPlayerName) { PlayerName = InMyPlayerName; };
    void SetPlayerName(const FString& InMyPlayerName) { PlayerName = InMyPlayerName; };
    FString GetPlayerName() { return PlayerName; }
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator|Customization")
    int32 SelectedHairIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator|Customization")
    int32 SelectedTopIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Operator|Customization")
    int32 SelectedBottomIndex = 0;

protected:
    IOnlineSessionPtr SessionInterface;

    TSharedPtr<FOnlineSessionSearch> SessionSearch;
    FOnlineSessionSearchResult LastJoinedSearchResult;
    
    FString TargetPassword;
    FString TargetRoomName;
    bool bIsSearchingPrivate = false;

    FTSTicker::FDelegateHandle TickerHandle;
    TWeakObjectPtr<UUserWidget> BoundJoinMenu;
    TWeakObjectPtr<UButton> BoundPrivateJoinBtn;

    bool TickWidgetBindings(float DeltaTime);
    UUserWidget* FindActiveWidget(const FString& ClassSubstr) const;
    void UpdatePlayerNameFromActiveMenu();
    void PopulateServerList();

    
    void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
    void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
    void OnFindSessionsComplete(bool bWasSuccessful);
};