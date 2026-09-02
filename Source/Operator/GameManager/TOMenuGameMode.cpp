// Fill out your copyright notice in the Description page of Project Settings.

#include "TOMenuGameMode.h"
#include "TOMenuPlayerController.h"
#include "Blueprint/UserWidget.h"

ATOMenuGameMode::ATOMenuGameMode()
{
	PlayerControllerClass = ATOMenuPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
	HUDClass = nullptr;
}

void ATOMenuGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (MainMenuWidgetClass)
	{
		MainMenuWidgetInstance = CreateWidget<UUserWidget>(GetWorld(), MainMenuWidgetClass);
		if (MainMenuWidgetInstance)
		{
			MainMenuWidgetInstance->AddToViewport();
		}
	}
}
