/**
* Copyright (C) 2017-2020 | Dry Eel Development
*
* Official EOSCore Documentation: https://eeldev.com
*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/CoreOnline.h"
#include "Interfaces/OnlineStoreInterfaceV2.h"
#include "Interfaces/OnlinePurchaseInterface.h"
#include "OnlineSubsystemPackageEOS.h"
#include "OnlineSubsystemTypesEOS.h"
#include "eos_ecom_types.h"

class UWorld;

class FOnlineStoreEOS : public IOnlineStoreV2, public IOnlinePurchase, public TSharedFromThis<FOnlineStoreEOS, ESPMode::ThreadSafe>
{
public:
	virtual ~FOnlineStoreEOS() = default;
private:
	FOnlineStoreEOS() = delete;

public:
// Begin IOnlineStoreV2
	virtual void QueryCategories(const FUniqueNetId& UserId, const FOnQueryOnlineStoreCategoriesComplete& Delegate) override;
	virtual void GetCategories(TArray<FOnlineStoreCategory>& OutCategories) const override;
	virtual void QueryOffersByFilter(const FUniqueNetId& UserId, const FOnlineStoreFilter& Filter, const FOnQueryOnlineStoreOffersComplete& Delegate) override;
	virtual void QueryOffersById(const FUniqueNetId& UserId, const TArray<FUniqueOfferId>& OfferIds, const FOnQueryOnlineStoreOffersComplete& Delegate) override;
	virtual void GetOffers(TArray<FOnlineStoreOfferRef>& OutOffers) const override;
	virtual TSharedPtr<FOnlineStoreOffer> GetOffer(const FUniqueOfferId& OfferId) const override;
// End IOnlineStoreV2

public:
// Begin IOnlinePurchase
	virtual bool IsAllowedToPurchase(const FUniqueNetId& UserId) override { return true; }
	virtual void Checkout(const FUniqueNetId& UserId, const FPurchaseCheckoutRequest& CheckoutRequest, const FOnPurchaseCheckoutComplete& Delegate) override;
	virtual void FinalizePurchase(const FUniqueNetId& UserId, const FString& ReceiptId) override;
	virtual void RedeemCode(const FUniqueNetId& UserId, const FRedeemCodeRequest& RedeemCodeRequest, const FOnPurchaseRedeemCodeComplete& Delegate) override;
	virtual void QueryReceipts(const FUniqueNetId& UserId, bool bRestoreReceipts, const FOnQueryReceiptsComplete& Delegate) override;
	virtual void GetReceipts(const FUniqueNetId& UserId, TArray<FPurchaseReceipt>& OutReceipts) const override;
	virtual void FinalizeReceiptValidationInfo(const FUniqueNetId& UserId, FString& InReceiptValidationInfo, const FOnFinalizeReceiptValidationInfoComplete& Delegate) override;
// End IOnlinePurchase

PACKAGE_SCOPE:
	FOnlineStoreEOS(FOnlineSubsystemEOS* InSubsystem);
	bool HandleEcomExec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar);

private:
	void QueryOffers(const FUniqueNetId& UserId, const FOnQueryOnlineStoreOffersComplete& Delegate);

private:
	FOnlineSubsystemEOS* m_Subsystem;
	TArray<FOnlineStoreOfferRef> m_CachedOffers;
	TArray<FUniqueOfferId> m_CachedOfferIds;
	TArray<FPurchaseReceipt> m_CachedReceipts;
};

typedef TSharedPtr<FOnlineStoreEOS, ESPMode::ThreadSafe> FOnlineStoreEOSPtr;
