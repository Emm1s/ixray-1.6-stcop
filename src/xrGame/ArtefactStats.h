#pragma once

#include "hit_immunity.h"

struct SArtefactStats
{
	CHitImmunity m_ArtefactHitImmunities;
	float m_additional_weight = 0.0f;
	float m_fHealthRestoreSpeed = 0.0f;
	float m_fRadiationRestoreSpeed = 0.0f;
	float m_fSatietyRestoreSpeed = 0.0f;
	float m_fThirstRestoreSpeed = 0.0f;
	float m_fSleepinessRestoreSpeed = 0.0f;
	float m_fPowerRestoreSpeed = 0.0f;
	float m_fBleedingRestoreSpeed = 0.0f;
	float m_fEquipmentDurabilityModifier = 1.0f;
	float m_fInventoryWeightModifier = 1.0f;
	float m_fJumpHeightModifier = 0.0f;
	float m_fMovementSpeedModifier = 0.0f;
	float m_fJumpSpeed = 0.0f;
	float m_fWalkAccel = 0.0f;
};
