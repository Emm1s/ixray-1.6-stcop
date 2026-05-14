#pragma once

#include "../xrCore/_types.h"

class CInventoryOwner;

enum class EPdaCommunicationStatus : u8
{
    Success = 0,
    DisabledByConfig,
    InvalidActor,
    InvalidNpc,
    NpcDead,
    NpcHostile,
    ActorBusy,
    NpcAlreadyTalking,
    NpcOffline,
    NpcOutOfRange,
    NpcNoCapability
};

// Owns only link/session state for PDA contacts (talk partners, eligibility).
// Phrase graphs and UI use CPhraseDialogManager + CUITalkWnd (same as face-to-face talk).
class CPdaCommunication final
{
public:
    CPdaCommunication();
    ~CPdaCommunication() = default;

    static CPdaCommunication& Get();

    bool IsEnabled() const;
    float GetTalkDistance() const;

    bool OpenDialog(CInventoryOwner* npc);
    EPdaCommunicationStatus CanStart(CInventoryOwner* npc, CInventoryOwner* actor) const;
    void Update();

    void Stop();

    bool IsSessionActive() const;
    bool IsSessionFor(CInventoryOwner* actor, const CInventoryOwner* npc) const;
    CInventoryOwner* GetSessionNpc() const;

private:
    bool BeginPdaSession(CInventoryOwner* npc);
    void EndPdaSession();
    bool IsNpcPdaEnabled(CInventoryOwner* npc) const;
    bool IsQuestTalkAllowed(CInventoryOwner* npc) const;
    bool IsNpcOnline(CInventoryOwner* npc) const;
    bool IsNpcHostileToActor(CInventoryOwner* npc, CInventoryOwner* actor) const;

private:
    void ensurePdaTalkConfigCache() const;

private:
    CInventoryOwner* _npc;
    CInventoryOwner* _actorOwner;

    bool _active;

    mutable bool _pdaTalkConfigLoaded;
    mutable bool _pdaTalkEnabledCached;
    mutable bool _pdaTalkIgnoreSwitchDistanceCached;
};

IC CPdaCommunication& PdaCommunication() { return CPdaCommunication::Get(); }
IC bool PdaCommunication_IsSessionActive() { return CPdaCommunication::Get().IsSessionActive(); }
IC void PdaCommunication_Stop() { CPdaCommunication::Get().Stop(); }
IC void PdaCommunication_Update() { CPdaCommunication::Get().Update(); }
