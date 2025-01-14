/**
* Copyright (C) 2017-2020 | Dry Eel Development
*
* Official EOSCore Documentation: https://eeldev.com
*/

#include "OnlineStoreEOS.h"
#include "OnlineSubsystemEOSPrivatePCH.h"
#include "OnlineSubsystemEOS.h"
#include "OnlineIdentityEOS.h"

#define ONLINE_ERROR_NAMESPACE "com.epicgames.oss.eos.error"

FOnlineStoreEOS::FOnlineStoreEOS(FOnlineSubsystemEOS* InSubsystem)
	: m_Subsystem(InSubsystem)
{
	check(m_Subsystem != nullptr);
}

void FOnlineStoreEOS::QueryCategories(const FUniqueNetId& UserId, const FOnQueryOnlineStoreCategoriesComplete& Delegate)
{
	Delegate.ExecuteIfBound(false, TEXT("QueryCategories Not Implemented"));
}

void FOnlineStoreEOS::GetCategories(TArray<FOnlineStoreCategory>& OutCategories) const
{
	OutCategories.Reset();
}

void FOnlineStoreEOS::QueryOffersByFilter(const FUniqueNetId& UserId, const FOnlineStoreFilter& Filter, const FOnQueryOnlineStoreOffersComplete& Delegate)
{
	QueryOffers(UserId, Delegate);
}

void FOnlineStoreEOS::QueryOffersById(const FUniqueNetId& UserId, const TArray<FUniqueOfferId>& OfferIds, const FOnQueryOnlineStoreOffersComplete& Delegate)
{
	QueryOffers(UserId, Delegate);
}

typedef TEOSCallback<EOS_Ecom_OnQueryOffersCallback, EOS_Ecom_QueryOffersCallbackInfo> FQueryOffersCallback;

void FOnlineStoreEOS::QueryOffers(const FUniqueNetId& UserId, const FOnQueryOnlineStoreOffersComplete& Delegate)
{
	if (m_CachedOfferIds.Num() && m_CachedOffers.Num())
	{
		Delegate.ExecuteIfBound(true, m_CachedOfferIds, TEXT("Returning cached offers"));
		return;
	}
	EOS_EpicAccountId AccountId = m_Subsystem->m_IdentityInterfacePtr->GetEpicAccountId(UserId);
	if (AccountId == nullptr)
	{
		Delegate.ExecuteIfBound(false, TArray<FUniqueOfferId>(), TEXT("Can't query offers for a null user"));
		return;
	}

	m_CachedOfferIds.Reset();
	m_CachedOffers.Reset();

	EOS_Ecom_QueryOffersOptions Options = { };
	Options.ApiVersion = EOS_ECOM_QUERYOFFERS_API_LATEST;
	Options.LocalUserId = AccountId;

	FQueryOffersCallback* CallbackObj = new FQueryOffersCallback();
	CallbackObj->m_CallbackLambda = [this, OnComplete = FOnQueryOnlineStoreOffersComplete(Delegate)](const EOS_Ecom_QueryOffersCallbackInfo* Data)
	{
		EOS_EResult Result = Data->ResultCode;
		if (Result != EOS_EResult::EOS_Success)
		{
			OnComplete.ExecuteIfBound(false, m_CachedOfferIds, EOS_EResult_ToString(Data->ResultCode));
			return;
		}

		EOS_Ecom_GetOfferCountOptions CountOptions = { };
		CountOptions.ApiVersion = EOS_ECOM_GETOFFERCOUNT_API_LATEST;
		CountOptions.LocalUserId = Data->LocalUserId;
		uint32 OfferCount = EOS_Ecom_GetOfferCount(m_Subsystem->m_EcomHandle, &CountOptions);

		EOS_Ecom_CopyOfferByIndexOptions OfferOptions = { };
		OfferOptions.ApiVersion = EOS_ECOM_COPYOFFERBYINDEX_API_LATEST;
		OfferOptions.LocalUserId = Data->LocalUserId;
		// Iterate and parse the offer list
		for (uint32 OfferIndex = 0; OfferIndex < OfferCount; OfferIndex++)
		{
			EOS_Ecom_CatalogOffer* Offer = nullptr;
			OfferOptions.OfferIndex = OfferIndex;
			EOS_EResult OfferResult = EOS_Ecom_CopyOfferByIndex(m_Subsystem->m_EcomHandle, &OfferOptions, &Offer);
			if (OfferResult != EOS_EResult::EOS_Success)
			{
				continue;
			}
			FOnlineStoreOfferRef OfferRef(new FOnlineStoreOffer());
			OfferRef->OfferId = Offer->Id;

			OfferRef->Title = FText::FromString(Offer->TitleText);
			OfferRef->Description = FText::FromString(Offer->DescriptionText);
			OfferRef->LongDescription = FText::FromString(Offer->LongDescriptionText);

			OfferRef->ExpirationDate = FDateTime(Offer->ExpirationTimestamp);

			OfferRef->CurrencyCode = Offer->CurrencyCode;
			if (Offer->PriceResult == EOS_EResult::EOS_Success)
			{
				OfferRef->RegularPrice = Offer->OriginalPrice;
				OfferRef->NumericPrice = Offer->CurrentPrice;
				OfferRef->DiscountType = Offer->DiscountPercentage == 0 ? EOnlineStoreOfferDiscountType::NotOnSale : EOnlineStoreOfferDiscountType::DiscountAmount;
			}

			m_CachedOffers.Add(OfferRef);
			m_CachedOfferIds.Add(OfferRef->OfferId);

			EOS_Ecom_CatalogOffer_Release(Offer);
		}

		OnComplete.ExecuteIfBound(true, m_CachedOfferIds, TEXT(""));
	};
	EOS_Ecom_QueryOffers(m_Subsystem->m_EcomHandle, &Options, CallbackObj, CallbackObj->GetCallbackPtr());
}

void FOnlineStoreEOS::GetOffers(TArray<FOnlineStoreOfferRef>& OutOffers) const
{
	OutOffers = m_CachedOffers;
}

TSharedPtr<FOnlineStoreOffer> FOnlineStoreEOS::GetOffer(const FUniqueOfferId& OfferId) const
{
	for (FOnlineStoreOfferRef Offer : m_CachedOffers)
	{
		if (Offer->OfferId == OfferId)
		{
			return Offer;
		}
	}
	return nullptr;
}

typedef TEOSCallback<EOS_Ecom_OnCheckoutCallback, EOS_Ecom_CheckoutCallbackInfo> FCheckoutCallback;

void FOnlineStoreEOS::Checkout(const FUniqueNetId& UserId, const FPurchaseCheckoutRequest& CheckoutRequest, const FOnPurchaseCheckoutComplete& Delegate)
{
	EOS_EpicAccountId AccountId = m_Subsystem->m_IdentityInterfacePtr->GetEpicAccountId(UserId);
	if (AccountId == nullptr)
	{
		UE_LOG_ONLINE(Error, TEXT("Checkout: failed due to invalid user"));
		Delegate.ExecuteIfBound(ONLINE_ERROR(EOnlineErrorResult::InvalidUser), MakeShared<FPurchaseReceipt>());
		return;
	}
	if (CheckoutRequest.PurchaseOffers.Num() == 0)
	{
		UE_LOG_ONLINE(Error, TEXT("Checkout: failed due to no items to buy"));
		Delegate.ExecuteIfBound(ONLINE_ERROR(EOnlineErrorResult::InvalidParams), MakeShared<FPurchaseReceipt>());
		return;
	}
	if (CheckoutRequest.PurchaseOffers.Num() > EOS_ECOM_CHECKOUT_MAX_ENTRIES)
	{
		UE_LOG_ONLINE(Error, TEXT("Checkout: can only buy %d items at a time"), EOS_ECOM_CHECKOUT_MAX_ENTRIES);
		Delegate.ExecuteIfBound(ONLINE_ERROR(EOnlineErrorResult::InvalidParams), MakeShared<FPurchaseReceipt>());
		return;
	}

	const int32 NumItems = CheckoutRequest.PurchaseOffers.Num();
	char Items[EOS_ECOM_CHECKOUT_MAX_ENTRIES][EOS_ECOM_TRANSACTIONID_MAXIMUM_LENGTH];
	TArray<EOS_Ecom_CheckoutEntry> Entries;
	Entries.AddZeroed(NumItems);

	// Convert the items
	for (int32 Index = 0; Index < NumItems; Index++)
	{
		Entries[Index].ApiVersion = EOS_ECOM_CHECKOUTENTRY_API_LATEST;
		Entries[Index].OfferId = Items[Index];
		FCStringAnsi::Strncpy(Items[Index], TCHAR_TO_UTF8(*CheckoutRequest.PurchaseOffers[Index].OfferId), EOS_ECOM_TRANSACTIONID_MAXIMUM_LENGTH);
	}

	EOS_Ecom_CheckoutOptions Options = { };
	Options.ApiVersion = EOS_ECOM_CHECKOUT_API_LATEST;
	Options.LocalUserId = AccountId;
	Options.EntryCount = NumItems;
	Options.Entries = (const EOS_Ecom_CheckoutEntry*)Entries.GetData();

	FCheckoutCallback* CallbackObj = new FCheckoutCallback();
	CallbackObj->m_CallbackLambda = [this, OnComplete = FOnPurchaseCheckoutComplete(Delegate)](const EOS_Ecom_CheckoutCallbackInfo* Data)
	{
		EOS_EResult Result = Data->ResultCode;
		if (Result != EOS_EResult::EOS_Success)
		{
			UE_LOG_ONLINE(Error, TEXT("EOS_Ecom_Checkout: failed with error (%s)"), ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
			OnComplete.ExecuteIfBound(ONLINE_ERROR(EOnlineErrorResult::Unknown), MakeShared<FPurchaseReceipt>());
			return;
		}

		// Update the cached receipts
		QueryReceipts(*m_Subsystem->m_IdentityInterfacePtr->GetLocalUniqueNetIdEOS(Data->LocalUserId), true,
			FOnQueryReceiptsComplete::CreateLambda([this, PurchaseComplete = FOnPurchaseCheckoutComplete(OnComplete), TransId = FString(Data->TransactionId)](const FOnlineError& Result)
		{
			if (!Result.WasSuccessful())
			{
				UE_LOG_ONLINE(Error, TEXT("EOS_Ecom_Checkout: failed to query receipts after purchase"));
				PurchaseComplete.ExecuteIfBound(ONLINE_ERROR(EOnlineErrorResult::Unknown), MakeShared<FPurchaseReceipt>());
				return;
			}

			TSharedRef<FPurchaseReceipt> Receipt = MakeShared<FPurchaseReceipt>();
			// Find the transaction in our receipts
			for (const FPurchaseReceipt& SearchReceipt : m_CachedReceipts)
			{
				if (SearchReceipt.TransactionId == TransId)
				{
					Receipt = MakeShared<FPurchaseReceipt>(SearchReceipt);
					break;
				}
			}
			PurchaseComplete.ExecuteIfBound(ONLINE_ERROR(EOnlineErrorResult::Success), Receipt);
		}));

	};
	EOS_Ecom_Checkout(m_Subsystem->m_EcomHandle, &Options, CallbackObj, CallbackObj->GetCallbackPtr());

}

void FOnlineStoreEOS::FinalizePurchase(const FUniqueNetId& UserId, const FString& ReceiptId)
{
	UE_LOG_ONLINE(Error, TEXT("FinalizePurchase: Not supported. Did you mean FinalizeReceiptValidationInfo?"));
}

void FOnlineStoreEOS::RedeemCode(const FUniqueNetId& UserId, const FRedeemCodeRequest& RedeemCodeRequest, const FOnPurchaseRedeemCodeComplete& Delegate)
{
	static const TSharedRef<FPurchaseReceipt> BlankReceipt(MakeShared<FPurchaseReceipt>());
	Delegate.ExecuteIfBound(ONLINE_ERROR(EOnlineErrorResult::NotImplemented), BlankReceipt);
}

typedef TEOSCallback<EOS_Ecom_OnQueryEntitlementsCallback, EOS_Ecom_QueryEntitlementsCallbackInfo> FQueryReceiptsCallback;

void FOnlineStoreEOS::QueryReceipts(const FUniqueNetId& UserId, bool bRestoreReceipts, const FOnQueryReceiptsComplete& Delegate)
{
	EOS_EpicAccountId AccountId = m_Subsystem->m_IdentityInterfacePtr->GetEpicAccountId(UserId);
	if (AccountId == nullptr)
	{
		UE_LOG_ONLINE(Error, TEXT("QueryReceipts: failed due to invalid user"));
		Delegate.ExecuteIfBound(ONLINE_ERROR(EOnlineErrorResult::InvalidUser));
		return;
	}

	m_CachedReceipts.Reset();

	EOS_Ecom_QueryEntitlementsOptions Options = { };
	Options.ApiVersion = EOS_ECOM_QUERYENTITLEMENTS_API_LATEST;
	Options.LocalUserId = AccountId;
	Options.bIncludeRedeemed = bRestoreReceipts ? EOS_TRUE : EOS_FALSE;

	FQueryReceiptsCallback* CallbackObj = new FQueryReceiptsCallback();
	CallbackObj->m_CallbackLambda = [this, OnComplete = FOnQueryReceiptsComplete(Delegate)](const EOS_Ecom_QueryEntitlementsCallbackInfo* Data)
	{
		EOS_EResult Result = Data->ResultCode;
		if (Result != EOS_EResult::EOS_Success)
		{
			UE_LOG_ONLINE(Error, TEXT("EOS_Ecom_QueryEntitlements: failed with error (%s)"), ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
			OnComplete.ExecuteIfBound(ONLINE_ERROR(EOnlineErrorResult::Unknown));
			return;
		}

		EOS_Ecom_GetEntitlementsCountOptions CountOptions = { };
		CountOptions.ApiVersion = EOS_ECOM_GETENTITLEMENTSCOUNT_API_LATEST;
		CountOptions.LocalUserId = Data->LocalUserId;
		uint32 Count = EOS_Ecom_GetEntitlementsCount(m_Subsystem->m_EcomHandle, &CountOptions);
		m_CachedReceipts.AddZeroed(Count);

		EOS_Ecom_CopyEntitlementByIndexOptions CopyOptions = { };
		CopyOptions.ApiVersion = EOS_ECOM_COPYENTITLEMENTBYINDEX_API_LATEST;
		CopyOptions.LocalUserId = Data->LocalUserId;

		for (uint32 Index = 0; Index < Count; Index++)
		{
			CopyOptions.EntitlementIndex = Index;

			EOS_Ecom_Entitlement* Receipt = nullptr;
			EOS_EResult CopyResult = EOS_Ecom_CopyEntitlementByIndex(m_Subsystem->m_EcomHandle, &CopyOptions, &Receipt);
			if (CopyResult != EOS_EResult::EOS_Success && CopyResult != EOS_EResult::EOS_Ecom_EntitlementStale)
			{
				UE_LOG_ONLINE(Error, TEXT("EOS_Ecom_CopyEntitlementByIndex: failed with error (%s)"), ANSI_TO_TCHAR(EOS_EResult_ToString(CopyResult)));
				continue;
			}

			// Parse the entitlement into the receipt format
			FPurchaseReceipt& PurchaseReceipt = m_CachedReceipts[Index];
			PurchaseReceipt.TransactionId = Receipt->EntitlementId;
			PurchaseReceipt.TransactionState = EPurchaseTransactionState::Purchased;
			PurchaseReceipt.AddReceiptOffer(FOfferNamespace(), Receipt->CatalogItemId, 1);
			FPurchaseReceipt::FLineItemInfo& LineItem = PurchaseReceipt.ReceiptOffers[0].LineItems.Emplace_GetRef();
			LineItem.ItemName = Receipt->EntitlementName;
			LineItem.UniqueId = Receipt->EntitlementId;
			LineItem.ValidationInfo = Receipt->bRedeemed == EOS_TRUE ? "" : Receipt->EntitlementId;

			EOS_Ecom_Entitlement_Release(Receipt);
		}

		OnComplete.ExecuteIfBound(ONLINE_ERROR(EOnlineErrorResult::Success));
	};
	EOS_Ecom_QueryEntitlements(m_Subsystem->m_EcomHandle, &Options, CallbackObj, CallbackObj->GetCallbackPtr());
}

void FOnlineStoreEOS::GetReceipts(const FUniqueNetId& UserId, TArray<FPurchaseReceipt>& OutReceipts) const
{
	OutReceipts = m_CachedReceipts;
}

typedef TEOSCallback<EOS_Ecom_OnRedeemEntitlementsCallback, EOS_Ecom_RedeemEntitlementsCallbackInfo> FRedeemReceiptCallback;

void FOnlineStoreEOS::FinalizeReceiptValidationInfo(const FUniqueNetId& UserId, FString& InReceiptValidationInfo, const FOnFinalizeReceiptValidationInfoComplete& Delegate)
{
	EOS_EpicAccountId AccountId = m_Subsystem->m_IdentityInterfacePtr->GetEpicAccountId(UserId);
	if (AccountId == nullptr)
	{
		Delegate.ExecuteIfBound(ONLINE_ERROR(EOnlineErrorResult::InvalidUser), InReceiptValidationInfo);
		return;
	}
	if (InReceiptValidationInfo.IsEmpty())
	{
		Delegate.ExecuteIfBound(ONLINE_ERROR(EOnlineErrorResult::InvalidParams), InReceiptValidationInfo);
		return;
	}

	char const* Ids[1];
	FTCHARToUTF8 EntitlementId(*InReceiptValidationInfo);
	Ids[0] = EntitlementId.Get();

	EOS_Ecom_RedeemEntitlementsOptions Options = { };
	Options.ApiVersion = EOS_ECOM_REDEEMENTITLEMENTS_API_LATEST;
	Options.LocalUserId = AccountId;
	Options.EntitlementIdCount = 1;
	Options.EntitlementIds = Ids;

	FRedeemReceiptCallback* CallbackObj = new FRedeemReceiptCallback();
	CallbackObj->m_CallbackLambda = [this, Info = FString(InReceiptValidationInfo), OnComplete = FOnFinalizeReceiptValidationInfoComplete(Delegate)](const EOS_Ecom_RedeemEntitlementsCallbackInfo* Data)
	{
		EOS_EResult Result = Data->ResultCode;
		if (Result != EOS_EResult::EOS_Success)
		{
			UE_LOG_ONLINE(Error, TEXT("EOS_Ecom_RedeemEntitlements: failed with error (%s)"), ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
			OnComplete.ExecuteIfBound(ONLINE_ERROR(EOnlineErrorResult::Unknown), Info);
			return;
		}

		// Find the receipt in our list and mark as redeemed (clear the validation info)
		for (FPurchaseReceipt& SearchReceipt : m_CachedReceipts)
		{
			if (SearchReceipt.TransactionId == Info)
			{
				// Clearing this field tells the game it can't be redeemed
				SearchReceipt.ReceiptOffers[0].LineItems[0].ValidationInfo.Empty();
				break;
			}
		}

		OnComplete.ExecuteIfBound(ONLINE_ERROR(EOnlineErrorResult::Success), Info);
	};
	EOS_Ecom_RedeemEntitlements(m_Subsystem->m_EcomHandle, &Options, CallbackObj, CallbackObj->GetCallbackPtr());
}

bool FOnlineStoreEOS::HandleEcomExec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (FParse::Command(&Cmd, TEXT("OFFERS")))
	{
		QueryOffers(*m_Subsystem->m_IdentityInterfacePtr->GetLocalUniqueNetIdEOS(),
			FOnQueryOnlineStoreOffersComplete::CreateLambda([this](bool bWasSuccessful, const TArray<FUniqueOfferId>& OfferIds, const FString& ErrorStr)
		{
			UE_LOG_ONLINE(Error, TEXT("QueryOffers: %s with error (%s)"), bWasSuccessful ? TEXT("succeeded") : TEXT("failed"), *ErrorStr);

			for (const FUniqueOfferId& OfferId : OfferIds)
			{
				UE_LOG_ONLINE(Log, TEXT("OfferId: %s"), *OfferId);
			}
		}));
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("RECEIPTS")))
	{
		QueryReceipts(*m_Subsystem->m_IdentityInterfacePtr->GetLocalUniqueNetIdEOS(), false,
			FOnQueryReceiptsComplete::CreateLambda([this](const FOnlineError& Result)
		{
			UE_LOG_ONLINE(Log, TEXT("QueryReceipts: %s with error (%s)"), Result.WasSuccessful() ? TEXT("succeeded") : TEXT("failed"), *Result.GetErrorRaw());
			for (const FPurchaseReceipt& Receipt : m_CachedReceipts)
			{
				UE_LOG_ONLINE(Log, TEXT("Receipt: %s"), *Receipt.TransactionId);
				UE_LOG_ONLINE(Log, TEXT("\tOffer Id (%s), Quantity (%d)"), *Receipt.ReceiptOffers[0].OfferId, Receipt.ReceiptOffers[0].Quantity);
				UE_LOG_ONLINE(Log, TEXT("\tLine item (%s) is %s"), *Receipt.ReceiptOffers[0].LineItems[0].ItemName, Receipt.ReceiptOffers[0].LineItems[0].IsRedeemable() ? TEXT("redeemable") : TEXT("not redeemable"));
			}
		}));
		return true;
	}
	return false;
}
