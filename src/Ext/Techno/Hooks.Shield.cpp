#include "Body.h"
#include <InfantryClass.h>
#include <SpecificStructures.h>
#include <Utilities/Macro.h>
#include <Utilities/GeneralUtils.h>
#include <Ext/TechnoType/Body.h>
#include <Ext/WarheadType/Body.h>
#include <Ext/TEvent/Body.h>
#include <VoxClass.h>
#include <RadarEventClass.h>
#include <TacticalClass.h>
#include <Ext/House/Body.h>
#include <ScenarioClass.h>

namespace RD
{
	bool SkipLowDamageCheck = false;
}
// #issue 88 : shield logic
DEFINE_HOOK(0x701900, TechnoClass_ReceiveDamage_Shield, 0x6)
{
	GET(TechnoClass*, pThis, ECX);
	LEA_STACK(args_ReceiveDamage*, args, 0x4);

	auto const pRules = RulesExt::Global();
	auto const pWHExt = WarheadTypeExt::ExtMap.Find(args->WH);

	if (pRules->CombatAlert && *args->Damage > 1 && !pWHExt->CombatAlert_Suppress.Get(!pWHExt->Malicious || pWHExt->Nonprovocative))
	{
		do
		{
			const auto pHouse = pThis->Owner;

			if (!pHouse || (!pThis->IsOwnedByCurrentPlayer && !pHouse->IsInPlayerControl) || !pThis->IsInPlayfield)
				break;

			const auto pSourceHouse = args->SourceHouse;

			if (pRules->CombatAlert_SuppressIfAllyDamage && pHouse->IsAlliedWith(pSourceHouse))
				break;

			const auto pHouseExt = HouseExt::ExtMap.Find(pHouse);

			if (pHouseExt->CombatAlertTimer.HasTimeLeft())
				break;

			const auto pType = pThis->GetTechnoType();
			const auto pTypeExt = TechnoTypeExt::ExtMap.Find(pType);

			if (!pTypeExt || !pTypeExt->CombatAlert.Get(!pType->Insignificant && !pType->Spawned))
				break;

			const auto pBuilding = pThis->WhatAmI() == AbstractType::Building ? static_cast<BuildingClass*>(pThis) : nullptr;

			if (pRules->CombatAlert_IgnoreBuilding && pBuilding && !pTypeExt->CombatAlert_NotBuilding.Get(pBuilding->Type->IsVehicle()))
				break;

			const CoordStruct coordInMap = pThis->GetCoords();

			if (pRules->CombatAlert_SuppressIfInScreen)
			{
				TacticalClass* const pTactical = TacticalClass::Instance;
				const Point2D coordInScreen = pTactical->CoordsToScreen(coordInMap) - pTactical->TacticalPos;
				const RectangleStruct screenArea = DSurface::Composite->GetRect();

				if (screenArea.Width >= coordInScreen.X && screenArea.Height >= coordInScreen.Y && coordInScreen.X >= 0 && coordInScreen.Y >= 0) // check if the unit is in screen
					break;
			}

			pHouseExt->CombatAlertTimer.Start(pRules->CombatAlert_Interval);
			RadarEventClass::Create(RadarEventType::Combat, CellClass::Coord2Cell(coordInMap));
			int index = -1;

			if (!pRules->CombatAlert_MakeAVoice) // No one want to play two sound at a time, I guess?
				break;
			else if (pTypeExt->CombatAlert_UseFeedbackVoice.Get(pRules->CombatAlert_UseFeedbackVoice) && pType->VoiceFeedback.Count > 0) // Use VoiceFeedback first
				VocClass::PlayGlobal(pType->VoiceFeedback.GetItem(0), 0x2000, 1.0);
			else if (pTypeExt->CombatAlert_UseAttackVoice.Get(pRules->CombatAlert_UseAttackVoice) && pType->VoiceAttack.Count > 0) // Use VoiceAttack then
				VocClass::PlayGlobal(pType->VoiceAttack.GetItem(0), 0x2000, 1.0);
			else if (pTypeExt->CombatAlert_UseEVA.Get(pRules->CombatAlert_UseEVA)) // Use Eva finally
				index = pTypeExt->CombatAlert_EVA.Get(VoxClass::FindIndex((const char*)"EVA_UnitsInCombat"));

			if (index != -1)
				VoxClass::PlayIndex(index);
		}
		while (false);
	}

	if (!*args->Damage || args->IgnoreDefenses)
		return 0;

	//Calculate Damage Multiplier
	if (pWHExt)
	{
		const auto pFirerHouse = pThis->Owner;
		const auto pTargetHouse = args->SourceHouse;
		double multiplier = 1.0;

		if (!pFirerHouse || !pTargetHouse || !pFirerHouse->IsAlliedWith(pTargetHouse))
			multiplier = pWHExt->DamageEnemiesMultiplier.Get(pRules->DamageEnemiesMultiplier);
		else if (pFirerHouse != pTargetHouse)
			multiplier = pWHExt->DamageAlliesMultiplier.Get(pRules->DamageAlliesMultiplier);
		else
			multiplier = pWHExt->DamageOwnerMultiplier.Get(pRules->DamageOwnerMultiplier);

		if (multiplier != 1.0)
		{
			const int sgnDamage = *args->Damage > 0 ? 1 : -1;
			const int calculateDamage = static_cast<int>(*args->Damage * multiplier);
			*args->Damage = calculateDamage ? calculateDamage : sgnDamage;
		}
	}

	//Shield Receive Damage
	const auto pExt = TechnoExt::ExtMap.Find(pThis);

	if (const auto pShieldData = pExt->Shield.get())
	{
		if (!pShieldData->IsActive())
			return 0;

		const int nDamageLeft = pShieldData->ReceiveDamage(args);
		if (nDamageLeft >= 0)
		{
			*args->Damage = nDamageLeft;

			if (auto pTag = pThis->AttachedTag)
				pTag->RaiseEvent((TriggerEvent)PhobosTriggerEvent::ShieldBroken, pThis, CellStruct::Empty);
		}

		if (nDamageLeft == 0)
			RD::SkipLowDamageCheck = true;
	}

	return 0;
}

DEFINE_HOOK(0x7019D8, TechnoClass_ReceiveDamage_SkipLowDamageCheck, 0x5)
{
	if (RD::SkipLowDamageCheck)
	{
		RD::SkipLowDamageCheck = false;
	}
	else
	{
		// Restore overridden instructions
		GET(int*, nDamage, EBX);
		if (*nDamage < 1)
			*nDamage = 1;
	}

	return 0x7019E3;
}

DEFINE_HOOK_AGAIN(0x70CF39, TechnoClass_ReplaceArmorWithShields, 0x6) //TechnoClass_EvalThreatRating_Shield
DEFINE_HOOK_AGAIN(0x6F7D31, TechnoClass_ReplaceArmorWithShields, 0x6) //TechnoClass_CanAutoTargetObject_Shield
DEFINE_HOOK_AGAIN(0x6FCB64, TechnoClass_ReplaceArmorWithShields, 0x6) //TechnoClass_CanFire_Shield
DEFINE_HOOK(0x708AEB, TechnoClass_ReplaceArmorWithShields, 0x6) //TechnoClass_ShouldRetaliate_Shield
{
	WeaponTypeClass* pWeapon = nullptr;
	if (R->Origin() == 0x708AEB)
		pWeapon = R->ESI<WeaponTypeClass*>();
	else if (R->Origin() == 0x6F7D31)
		pWeapon = R->EBP<WeaponTypeClass*>();
	else
		pWeapon = R->EBX<WeaponTypeClass*>();

	ObjectClass* pTarget = nullptr;
	if (R->Origin() == 0x6F7D31 || R->Origin() == 0x70CF39)
		pTarget = R->ESI<ObjectClass*>();
	else
		pTarget = R->EBP<ObjectClass*>();

	if (const auto pExt = TechnoExt::ExtMap.Find(abstract_cast<TechnoClass*>(pTarget)))
	{
		if (const auto pShieldData = pExt->Shield.get())
		{
			if (pShieldData->CanBePenetrated(pWeapon->Warhead))
				return 0;

			if (pShieldData->IsActive())
			{
				R->EAX(pShieldData->GetArmorType());
				return R->Origin() + 6;
			}
		}
	}

	return 0;
}

DEFINE_HOOK(0x6F6AC4, TechnoClass_Remove_Shield, 0x5)
{
	GET(TechnoClass*, pThis, ECX);

	const auto pExt = TechnoExt::ExtMap.Find(pThis);

	if (pExt->Shield)
		pExt->Shield->KillAnim();

	return 0;
}

DEFINE_HOOK(0x6F65D1, TechnoClass_DrawHealthBar_DrawBuildingShieldBar, 0x6)
{
	GET(TechnoClass*, pThis, ESI);
	GET(int, length, EBX);
	GET_STACK(Point2D*, pLocation, STACK_OFFSET(0x4C, 0x4));
	GET_STACK(RectangleStruct*, pBound, STACK_OFFSET(0x4C, 0x8));

	const auto pExt = TechnoExt::ExtMap.Find(pThis);

	if (const auto pShieldData = pExt->Shield.get())
	{
		if (pShieldData->IsAvailable())
			pShieldData->DrawShieldBar(length, pLocation, pBound);
	}

	TechnoExt::ProcessDigitalDisplays(pThis);

	return 0;
}

DEFINE_HOOK(0x6F683C, TechnoClass_DrawHealthBar_DrawOtherShieldBar, 0x7)
{
	GET(TechnoClass*, pThis, ESI);
	GET_STACK(Point2D*, pLocation, STACK_OFFSET(0x4C, 0x4));
	GET_STACK(RectangleStruct*, pBound, STACK_OFFSET(0x4C, 0x8));

	const auto pExt = TechnoExt::ExtMap.Find(pThis);

	if (const auto pShieldData = pExt->Shield.get())
	{
		if (pShieldData->IsAvailable())
		{
			const int length = pThis->WhatAmI() == AbstractType::Infantry ? 8 : 17;
			pShieldData->DrawShieldBar(length, pLocation, pBound);
		}
	}

	TechnoExt::ProcessDigitalDisplays(pThis);

	return 0;
}

#pragma region HealingWeapons

#pragma region TechnoClass_EvaluateObject

namespace EvaluateObjectTemp
{
	WeaponTypeClass* PickedWeapon = nullptr;
}

DEFINE_HOOK(0x6F7E24, TechnoClass_EvaluateObject_SetContext, 0x6)
{
	GET(WeaponTypeClass*, pWeapon, EBP);

	EvaluateObjectTemp::PickedWeapon = pWeapon;

	return 0;
}

double __fastcall HealthRatio_Wrapper(TechnoClass* pTechno)
{
	double result = pTechno->GetHealthPercentage();

	if (result >= 1.0)
	{
		if (const auto pExt = TechnoExt::ExtMap.Find(pTechno))
		{
			if (const auto pShieldData = pExt->Shield.get())
			{
				if (pShieldData->IsActive())
				{
					const auto pWH = EvaluateObjectTemp::PickedWeapon ? EvaluateObjectTemp::PickedWeapon->Warhead : nullptr;
					const auto pFoot = abstract_cast<FootClass*>(pTechno);

					if (!pShieldData->CanBePenetrated(pWH) || ((pFoot && pFoot->ParasiteEatingMe)))
						result = pExt->Shield->GetHealthRatio();
				}
			}
		}
	}

	return result;
}

DEFINE_JUMP(CALL, 0x6F7F51, GET_OFFSET(HealthRatio_Wrapper))

#pragma endregion TechnoClass_EvaluateObject

class AresScheme
{
	static inline ObjectClass* LinkedObj = nullptr;
public:
	static void __cdecl Prefix(TechnoClass* pThis, ObjectClass* pObj, int nWeaponIndex, bool considerEngineers)
	{
		if (LinkedObj)
			return;

		if (considerEngineers && CanApplyEngineerActions(pThis, pObj))
			return;

		if (nWeaponIndex < 0)
			nWeaponIndex = pThis->SelectWeapon(pObj);

		if (const auto pTechno = abstract_cast<TechnoClass*>(pObj))
		{
			if (const auto pExt = TechnoExt::ExtMap.Find(pTechno))
			{
				if (const auto pShieldData = pExt->Shield.get())
				{
					if (pShieldData->IsActive())
					{
						const auto pWeapon = pThis->GetWeapon(nWeaponIndex)->WeaponType;
						const auto pFoot = abstract_cast<FootClass*>(pObj);

						if (pWeapon && (!pShieldData->CanBePenetrated(pWeapon->Warhead) || (pFoot && pFoot->ParasiteEatingMe)))
						{
							const auto shieldRatio = pExt->Shield->GetHealthRatio();

							if (shieldRatio < 1.0)
							{
								LinkedObj = pObj;
								--LinkedObj->Health;
							}
						}
					}
				}
			}
		}
	}

	static void __cdecl Suffix()
	{
		if (LinkedObj)
		{
			++LinkedObj->Health;
			LinkedObj = nullptr;
		}
	}

private:
	static bool CanApplyEngineerActions(TechnoClass* pThis, ObjectClass* pTarget)
	{
		const auto pInf = abstract_cast<InfantryClass*>(pThis);
		const auto pBuilding = abstract_cast<BuildingClass*>(pTarget);

		if (!pInf || !pBuilding)
			return false;

		bool allied = HouseClass::CurrentPlayer->IsAlliedWith(pBuilding);

		if (allied && pBuilding->Type->Repairable)
			return true;

		if (!allied && pBuilding->Type->Capturable &&
			(!pBuilding->Owner->Type->MultiplayPassive || !pBuilding->Type->CanBeOccupied || pBuilding->IsBeingWarpedOut()))
		{
			return true;
		}

		return false;
	}
};


FireError __fastcall UnitClass__GetFireError_Wrapper(UnitClass* pThis, void* _, ObjectClass* pObj, int nWeaponIndex, bool ignoreRange)
{
	AresScheme::Prefix(pThis, pObj, nWeaponIndex, false);
	auto const result = pThis->UnitClass::GetFireError(pObj, nWeaponIndex, ignoreRange);
	AresScheme::Suffix();
	return result;
}
DEFINE_JUMP(VTABLE, 0x7F6030, GET_OFFSET(UnitClass__GetFireError_Wrapper))

FireError __fastcall InfantryClass__GetFireError_Wrapper(InfantryClass* pThis, void* _, ObjectClass* pObj, int nWeaponIndex, bool ignoreRange)
{
	AresScheme::Prefix(pThis, pObj, nWeaponIndex, false);
	auto const result = pThis->InfantryClass::GetFireError(pObj, nWeaponIndex, ignoreRange);
	AresScheme::Suffix();
	return result;
}
DEFINE_JUMP(VTABLE, 0x7EB418, GET_OFFSET(InfantryClass__GetFireError_Wrapper))

Action __fastcall UnitClass__WhatAction_Wrapper(UnitClass* pThis, void* _, ObjectClass* pObj, bool ignoreForce)
{
	AresScheme::Prefix(pThis, pObj, -1, false);
	auto const result = pThis->UnitClass::MouseOverObject(pObj, ignoreForce);
	AresScheme::Suffix();
	return result;
}
DEFINE_JUMP(VTABLE, 0x7F5CE4, GET_OFFSET(UnitClass__WhatAction_Wrapper))

Action __fastcall InfantryClass__WhatAction_Wrapper(InfantryClass* pThis, void* _, ObjectClass* pObj, bool ignoreForce)
{
	AresScheme::Prefix(pThis, pObj, -1, pThis->Type->Engineer);
	auto const result = pThis->InfantryClass::MouseOverObject(pObj, ignoreForce);
	AresScheme::Suffix();
	return result;
}
DEFINE_JUMP(VTABLE, 0x7EB0CC, GET_OFFSET(InfantryClass__WhatAction_Wrapper))
#pragma endregion HealingWeapons
