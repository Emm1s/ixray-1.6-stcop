#include "stdafx.h"

#include "pda_communication.h"

#include "../xrCore/EngineExternal.h"
#include "Actor.h"
#include "GameObject.h"
#include "InventoryOwner.h"
#include "Level.h"
#include "relation_registry.h"

#ifdef DEBUG
#define PDA_LOG(...) Msg(__VA_ARGS__)
#else
#define PDA_LOG(...) ((void)0)
#endif

CPdaCommunication::CPdaCommunication() :
    _npc(nullptr),
    _actorOwner(nullptr),
    _active(false),
    _pdaTalkConfigLoaded(false),
    _pdaTalkEnabledCached(false),
    _pdaTalkIgnoreSwitchDistanceCached(false)
{
}

CPdaCommunication& CPdaCommunication::Get()
{
    static CPdaCommunication communication;
    return communication;
}

void CPdaCommunication::ensurePdaTalkConfigCache() const
{
    if (_pdaTalkConfigLoaded)
    {
        return;
    }
    _pdaTalkConfigLoaded = true;
    CInifile* ini = EngineExternal().GetIniFile();
    if (ini == nullptr || !ini->section_exist("pda_talk"))
    {
        _pdaTalkEnabledCached = false;
        _pdaTalkIgnoreSwitchDistanceCached = false;
        return;
    }
    _pdaTalkEnabledCached = READ_IF_EXISTS(ini, r_bool, "pda_talk", "enabled", false);
    _pdaTalkIgnoreSwitchDistanceCached = READ_IF_EXISTS(ini, r_bool, "pda_talk", "ignore_switch_distance", true);
}

bool CPdaCommunication::IsEnabled() const
{
    ensurePdaTalkConfigCache();
    return _pdaTalkEnabledCached;
}

float CPdaCommunication::GetTalkDistance() const
{
    if (pSettings && pSettings->section_exist("switch_distance"))
    {
        return pSettings->r_float("switch_distance", "switch_distance");
    }

    return 150.0f;
}

void CPdaCommunication::Update()
{
    if (_active)
    {
        CActor* actor = Actor();
        if (!actor || CanStart(_npc, actor) != EPdaCommunicationStatus::Success)
        {
            Stop();
            return;
        }
    }
}

bool CPdaCommunication::OpenDialog(CInventoryOwner* npc)
{
    CActor* actor = Actor();

    if (_active && _npc == npc)
    {
        return true;
    }

    if (_active && _npc != npc)
    {
        Stop();
    }

    const EPdaCommunicationStatus status = CanStart(npc, actor);
    if (status != EPdaCommunicationStatus::Success)
    {
        return false;
    }

    if (!BeginPdaSession(npc))
    {
        return false;
    }

    _actorOwner = actor ? actor->cast_inventory_owner() : nullptr;
    _npc = npc;
    _active = true;
    PDA_LOG("[PDA] Session started with %s", npc ? npc->Name() : "?");
    return true;
}

bool CPdaCommunication::BeginPdaSession(CInventoryOwner* npc)
{
    CActor* actor = Actor();
    if (!actor || !npc)
    {
        return false;
    }

    actor->SetTalkPartner(npc);
    npc->SetTalkPartner(actor);
    PDA_LOG("[PDA] BeginPdaSession actor=%s npc=%s", actor->Name(), npc->Name());
    return true;
}

void CPdaCommunication::EndPdaSession()
{
    CActor* actor = Actor();
    if (actor && _npc)
    {
        if (actor->GetTalkPartner() == _npc)
        {
            actor->SetTalkPartner(nullptr);
        }
        if (_npc->GetTalkPartner() == actor)
        {
            _npc->SetTalkPartner(nullptr);
        }
    }
}

void CPdaCommunication::Stop()
{
    EndPdaSession();
    _active = false;
    _npc = nullptr;
    _actorOwner = nullptr;
}

bool CPdaCommunication::IsSessionActive() const
{
    return _active;
}

bool CPdaCommunication::IsSessionFor(CInventoryOwner* actor, const CInventoryOwner* npc) const
{
    if (!_active || !actor || !npc)
    {
        return false;
    }

    return _actorOwner == actor && _npc == npc;
}

CInventoryOwner* CPdaCommunication::GetSessionNpc() const
{
    return _npc;
}

EPdaCommunicationStatus CPdaCommunication::CanStart(CInventoryOwner* npc, CInventoryOwner* actor) const
{
    if (!IsEnabled())
    {
        return EPdaCommunicationStatus::DisabledByConfig;
    }

    if (actor == nullptr || actor->cast_actor() == nullptr)
    {
        return EPdaCommunicationStatus::InvalidActor;
    }

    if (npc == nullptr || npc->cast_game_object() == nullptr)
    {
        return EPdaCommunicationStatus::InvalidNpc;
    }

    if (npc->IsTalking() && npc->GetTalkPartner() != actor)
    {
        return EPdaCommunicationStatus::NpcAlreadyTalking;
    }

    if (actor->IsTalking() && actor->GetTalkPartner() != npc)
    {
        return EPdaCommunicationStatus::ActorBusy;
    }

    CEntityAlive* npcAlive = npc->cast_entity_alive();
    if (npcAlive == nullptr || !npcAlive->g_Alive())
    {
        return EPdaCommunicationStatus::NpcDead;
    }

    if (!IsNpcOnline(npc))
    {
        return EPdaCommunicationStatus::NpcOffline;
    }

    if (IsNpcHostileToActor(npc, actor))
    {
        return EPdaCommunicationStatus::NpcHostile;
    }

    const CGameObject* actorObject = actor->cast_game_object();
    const CGameObject* npcObject = npc->cast_game_object();
    if (!actorObject || !npcObject)
    {
        return EPdaCommunicationStatus::InvalidNpc;
    }

    ensurePdaTalkConfigCache();
    if (!_pdaTalkIgnoreSwitchDistanceCached)
    {
        if (actorObject->Position().distance_to(npcObject->Position()) > GetTalkDistance())
        {
            return EPdaCommunicationStatus::NpcOutOfRange;
        }
    }

    if (!IsNpcPdaEnabled(npc))
    {
        return EPdaCommunicationStatus::NpcNoCapability;
    }

    if (!IsQuestTalkAllowed(npc))
    {
        return EPdaCommunicationStatus::NpcNoCapability;
    }

    return EPdaCommunicationStatus::Success;
}

bool CPdaCommunication::IsNpcOnline(CInventoryOwner* npc) const
{
    const CGameObject* gameObject = npc ? npc->cast_game_object() : nullptr;
    if (!gameObject)
    {
        return false;
    }

    return Level().Objects.net_Find(gameObject->ID()) != nullptr;
}

bool CPdaCommunication::IsNpcHostileToActor(CInventoryOwner* npc, CInventoryOwner* actor) const
{
    const ALife::ERelationType relation = RELATION_REGISTRY().GetRelationType(actor, npc);
    return relation == ALife::eRelationTypeEnemy || relation == ALife::eRelationTypeWorstEnemy;
}

bool CPdaCommunication::IsNpcPdaEnabled(CInventoryOwner* npc) const
{
    const CGameObject* gameObject = npc ? npc->cast_game_object() : nullptr;
    if (!gameObject || !pSettings || !pSettings->section_exist(gameObject->cNameSect()))
    {
        return true;
    }

    return READ_IF_EXISTS(pSettings, r_bool, gameObject->cNameSect(), "pda_talk_enabled", true);
}

bool CPdaCommunication::IsQuestTalkAllowed(CInventoryOwner* npc) const
{
    const CGameObject* gameObject = npc ? npc->cast_game_object() : nullptr;
    if (!gameObject || !pSettings || !pSettings->section_exist(gameObject->cNameSect()))
    {
        return true;
    }

    return READ_IF_EXISTS(pSettings, r_bool, gameObject->cNameSect(), "pda_quest_talk_enabled", true);
}

