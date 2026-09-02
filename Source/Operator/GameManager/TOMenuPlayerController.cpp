// Fill out your copyright notice in the Description page of Project Settings.

#include "TOMenuPlayerController.h"

ATOMenuPlayerController::ATOMenuPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}

void ATOMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	SetMenuInputMode();
}

void ATOMenuPlayerController::SetMenuInputMode()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	FInputModeUIOnly InputModeData;
	InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputModeData);
}

void ATOMenuPlayerController::SetGameAndUIInputMode()
{
	bShowMouseCursor = true;

	FInputModeGameAndUI InputModeData;
	InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputModeData.SetHideCursorDuringCapture(false);
	SetInputMode(InputModeData);
}
