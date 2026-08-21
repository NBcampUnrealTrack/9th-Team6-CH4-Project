// Fill out your copyright notice in the Description page of Project Settings.


#include "TOCharacter.h"

// Sets default values
ATOCharacter::ATOCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ATOCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ATOCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ATOCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

