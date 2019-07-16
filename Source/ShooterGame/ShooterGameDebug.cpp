// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "ShooterGameDebug.h"
#include "Kismet/KismetSystemLibrary.h"

DEFINE_LOG_CATEGORY(DebugLog);


ShooterGameDebug::ShooterGameDebug()
{
}

ShooterGameDebug::~ShooterGameDebug()
{
}

void ShooterGameDebug::Log(const char* Text,float displayTime)
{
	GEngine->AddOnScreenDebugMessage(-1, displayTime, FColor::Blue, FString(Text));
	UE_LOG(DebugLog, Log, TEXT("Log %s"), Text);
}
void ShooterGameDebug::Log(const FString &Text, float displayTime /* = 2.0f */)
{
	GEngine->AddOnScreenDebugMessage(-1, displayTime, FColor::Blue, Text);
	UE_LOG(DebugLog, Log, TEXT("Log %s"), *Text);
}


void ShooterGameDebug::LogWaring(const char* Text,float displayTime)
{
	GEngine->AddOnScreenDebugMessage(-1, displayTime, FColor::Yellow, FString(Text));
	UE_LOG(DebugLog, Warning, TEXT("Warning %s"), Text);
}
void ShooterGameDebug::LogWaring(const FString &Text, float displayTime /* = 2.0f */)
{
	GEngine->AddOnScreenDebugMessage(-1, displayTime, FColor::Yellow, Text);
	UE_LOG(DebugLog, Warning, TEXT("Warning %s"), *Text);
}

void ShooterGameDebug::LogError(const char* Text,float displayTime)
{
	GEngine->AddOnScreenDebugMessage(-1, displayTime, FColor::Red, FString(Text));
	UE_LOG(DebugLog, Error, TEXT("Error %s"), Text);	
}
void ShooterGameDebug::LogError(const FString &Text, float displayTime)
{
	GEngine->AddOnScreenDebugMessage(-1, displayTime, FColor::Red, Text);
	UE_LOG(DebugLog, Error, TEXT("Error %s"), *Text);
}

void ShooterGameDebug::PrintString(UObject* WorldContextObject,const FString &Text, float displayTime /* = 2.0f */)
{
	UKismetSystemLibrary::PrintString(WorldContextObject, Text, true, true, FLinearColor(0.0, 0.66, 1.0), displayTime);
}