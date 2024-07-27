#include "Body.h"

#include <Ext/Rules/Body.h>

#include <Utilities/Macro.h>
#include <Utilities/EnumFunctions.h>

DEFINE_HOOK(0x460285, BuildingTypeClass_LoadFromINI_Muzzle, 0x6)
{
	enum { Skip = 0x460388, Read = 0x460299 };

	GET(BuildingTypeClass*, pThis, EBP);

	// Restore overriden instructions
	R->Stack(STACK_OFFSET(0x368, -0x358), 0);
	R->EDX(0);

	// Disable Vanilla Muzzle flash when MaxNumberOccupants is 0 or more than 10
	return !pThis->MaxNumberOccupants || pThis->MaxNumberOccupants > 10
		? Skip : Read;
}

DEFINE_HOOK(0x44043D, BuildingClass_AI_Temporaled_Chronosparkle_MuzzleFix, 0x8)
{
	GET(BuildingClass*, pThis, ESI);

	auto pType = pThis->Type;
	if (pType->MaxNumberOccupants > 10)
	{
		GET(int, nFiringIndex, EBX);
		auto pTypeExt = BuildingTypeExt::ExtMap.Find(pType);
		R->EAX(&pTypeExt->OccupierMuzzleFlashes[nFiringIndex]);
	}

	return 0;
}

DEFINE_HOOK(0x45387A, BuildingClass_FireOffset_Replace_MuzzleFix, 0xA)
{
	GET(BuildingClass*, pThis, ESI);

	auto pType = pThis->Type;
	if (pType->MaxNumberOccupants > 10)
	{
		auto pTypeExt = BuildingTypeExt::ExtMap.Find(pType);
		R->EDX(&pTypeExt->OccupierMuzzleFlashes[pThis->FiringOccupantIndex]);
	}

	return 0;
}

DEFINE_HOOK(0x458623, BuildingClass_KillOccupiers_Replace_MuzzleFix, 0x7)
{
	GET(BuildingClass*, pThis, ESI);

	auto pType = pThis->Type;
	if (pType->MaxNumberOccupants > 10)
	{
		GET(int, nFiringIndex, EDI);
		auto pTypeExt = BuildingTypeExt::ExtMap.Find(pType);
		R->ECX(&pTypeExt->OccupierMuzzleFlashes[nFiringIndex]);
	}

	return 0;
}

DEFINE_HOOK(0x6F34B7, TechnoClass_WhatWeaponShouldIUse_AllowAirstrike, 0x6)
{
	enum { SkipGameCode = 0x6F34BD };

	GET(BuildingTypeClass*, pThis, ECX);

	const auto pExt = BuildingTypeExt::ExtMap.Find(pThis);
	R->EAX(pExt->AllowAirstrike.Get(pThis->CanC4));

	return SkipGameCode;
}

DEFINE_HOOK(0x51EAF2, TechnoClass_WhatAction_AllowAirstrike, 0x6)
{
	enum { SkipGameCode = 0x51EAF8 };

	GET(BuildingTypeClass*, pThis, ESI);

	const auto pExt = BuildingTypeExt::ExtMap.Find(pThis);
	R->EAX(pExt->AllowAirstrike.Get(pThis->CanC4));

	return SkipGameCode;
}

// Rewritten
DEFINE_HOOK(0x465D40, BuildingTypeClass_IsVehicle, 0x6)
{
	enum { ReturnFromFunction = 0x465D6A };

	GET(BuildingTypeClass*, pThis, ECX);

	const auto pExt = BuildingTypeExt::ExtMap.Find(pThis);

	if (pExt->ConsideredVehicle.isset())
	{
		R->EAX(pExt->ConsideredVehicle.Get());
		return ReturnFromFunction;
	}

	return 0;
}

DEFINE_HOOK(0x5F5416, ObjectClass_ReceiveDamage_CanC4DamageRounding, 0x6)
{
	enum { SkipGameCode = 0x5F5456 };

	GET(ObjectClass*, pThis, ESI);
	GET(int*, pDamage, EDI);

	if (*pDamage == 0 && pThis->WhatAmI() == AbstractType::Building)
	{
		auto const pType = static_cast<BuildingClass*>(pThis)->Type;

		if (!pType->CanC4)
		{
			auto const pTypeExt = BuildingTypeExt::ExtMap.Find(pType);

			if (!pTypeExt->CanC4_AllowZeroDamage)
				*pDamage = 1;
		}
	}

	return SkipGameCode;
}
