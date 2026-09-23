// Fill out your copyright notice in the Description page of Project Settings.


#include "Settings/A27OnlineSettings.h"

UA27OnlineSettings::UA27OnlineSettings()
{
	
	CategoryName = TEXT("Plugins");
	SectionName = TEXT("Also27 Online");
	
	SessionCodeValidCharacters = TEXT("ABCDEFGHJKLMNPQRSTUVWXYZ123456789");
	SessionCodeLength = 4;
}
