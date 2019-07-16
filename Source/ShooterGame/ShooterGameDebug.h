// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

//General Log
DECLARE_LOG_CATEGORY_EXTERN(DebugLog, Log, All);
//

/**
 * 
 */
class SHOOTERGAME_API ShooterGameDebug
{
public:
	ShooterGameDebug();
	~ShooterGameDebug();

	/*Log sring on streen and controller **/
	static void Log(const char* Text,float displayTime = 2.0f);
	static void Log(const FString &Text, float displayTime = 2.0f);

	/*Log waring sring on streen and controller **/
	static void LogWaring(const char* Text, float displayTime = 2.0f);
	static void LogWaring(const FString &Text, float displayTime = 2.0f);

	/*Log error sring on streen and controller **/
	static void LogError(const char* Text, float displayTime = 2.0f);
	static void LogError(const FString &Text, float displayTime = 2.0f);

	static void PrintString(UObject* WorldContextObject, const FString &Text, float displayTime = 2.0f);

};
