/**
* Copyright (C) 2017-2020 | Dry Eel Development
*
* Official EOSCore Documentation: https://eeldev.com
*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/CoreOnline.h"
#include "OnlineSubsystemTypes.h"
#include "OnlineSubsystemTypesEOS.h"
#include "eos_auth_types.h"
#include "eos_connect_types.h"

class FOnlineSubsystemEOS;

class FOnlineUserEOS : public IOnlineUser
{
public:
	FOnlineUserEOS(FOnlineSubsystemEOS* InSubsystem);
	virtual ~FOnlineUserEOS() {}
// IOnlineUser Interface
public:
	virtual bool QueryUserInfo(int32 LocalUserNum, const TArray<TSharedRef<const FUniqueNetId>>& UserIds) override;
	virtual bool GetAllUserInfo(int32 LocalUserNum, TArray<TSharedRef<class FOnlineUser>>& OutUsers) override;
	virtual TSharedPtr<FOnlineUser> GetUserInfo(int32 LocalUserNum, const class FUniqueNetId& UserId) override;
	virtual bool QueryUserIdMapping(const FUniqueNetId& UserId, const FString& DisplayNameOrEmail, const FOnQueryUserMappingComplete& Delegate = FOnQueryUserMappingComplete()) override;
	virtual bool QueryExternalIdMappings(const FUniqueNetId& UserId, const FExternalIdQueryOptions& QueryOptions, const TArray<FString>& ExternalIds, const FOnQueryExternalIdMappingsComplete& Delegate = FOnQueryExternalIdMappingsComplete()) override;
	virtual void GetExternalIdMappings(const FExternalIdQueryOptions& QueryOptions, const TArray<FString>& ExternalIds, TArray<TSharedPtr<const FUniqueNetId>>& OutIds) override;
	virtual TSharedPtr<const FUniqueNetId> GetExternalIdMapping(const FExternalIdQueryOptions& QueryOptions, const FString& ExternalId) override;
// ~IOnlineUser Interface
private:
	FOnlineSubsystemEOS* m_Subsystem;
};