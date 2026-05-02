#pragma once

namespace PdaConfig
{
constexpr const char* TabAliasesSection = "pda_tab_aliases";
constexpr const char* MapSubdialogWindowName = "map_wnd";
} // namespace PdaConfig

namespace PdaXml
{
constexpr const char* Main = "pda.xml";
constexpr const char* Map = "pda_map.xml";
constexpr const char* Ranking = "pda_ranking.xml";
constexpr const char* FactionWar = "pda_fraction_war.xml";
} // namespace PdaXml

namespace PdaSectionId
{
constexpr const char* Tasks = "eptTasks";
constexpr const char* Quests = "eptQuests";
constexpr const char* FractionWar = "eptFractionWar";
constexpr const char* Contacts = "eptContacts";
constexpr const char* Ranking = "eptRanking";
constexpr const char* RankingGlobal = "eptRankingGlobal";
constexpr const char* Logs = "eptLogs";
constexpr const char* Encyclopedia = "eptEncyclopedia";
constexpr const char* ActorStatistic = "eptActorStatistic";
constexpr const char* Diary = "eptDiary";
constexpr const char* Map = "eptMap";

const char* Resolve(const char* defaultId);
bool Equals(const shared_str& sectionId, const char* defaultId);
} // namespace PdaSectionId

namespace PdaActorInfo
{
constexpr const char* Show = "ui_pda";
constexpr const char* Hide = "ui_pda_hide";
} // namespace PdaActorInfo

namespace PdaLegacyTabId
{
constexpr const char* Legacy0 = "0";
constexpr const char* Legacy1 = "1";
constexpr const char* Legacy2 = "2";
constexpr const char* Legacy3 = "3";
constexpr const char* Legacy4 = "4";
constexpr const char* Legacy5 = "5";
constexpr const char* Legacy6 = "6";
} // namespace PdaLegacyTabId

namespace PdaScript
{
constexpr const char* OnSetActiveSubdialog = "OnSetActiveSubdialog";
constexpr const char* OnGetRankingsArraySize = "OnGetRankingsArraySize";
constexpr const char* GetMaxMemberCount = "pda.get_max_member_count";
constexpr const char* GetMaxResource = "pda.get_max_resource";
constexpr const char* GetMaxPower = "pda.get_max_power";
constexpr const char* GetValuableArtifactIcon = "pda.get_valuable_artifact_icon";
} // namespace PdaScript

namespace PdaNavButton
{
constexpr const char* Legend = "btn_nav_legend";
constexpr const char* ZoomIn = "btn_nav_zoom_in";
constexpr const char* Center = "btn_nav_center";
constexpr const char* ZoomOut = "btn_nav_zoom_out";
constexpr const char* ZoomReset = "btn_nav_zmreset";
constexpr const char* Up = "btn_nav_up";
constexpr const char* Down = "btn_nav_down";
constexpr const char* Left = "btn_nav_left";
constexpr const char* Right = "btn_nav_right";
constexpr const char* PersonalSpot = "btn_personal_spot";
} // namespace PdaNavButton

namespace PdaMapSpot
{
constexpr const char* Treasure = "treasure";
constexpr const char* PrimaryObject = "primary_object";
constexpr const char* SecondaryTask = "secondary_task_location";
constexpr const char* SecondaryTaskComplexTimer = "secondary_task_location_complex_timer";

constexpr const char* Trader = "ui_pda2_trader_location";
constexpr const char* Mechanic = "ui_pda2_mechanic_location";
constexpr const char* Scout = "ui_pda2_scout_location";
constexpr const char* QuestNpc = "ui_pda2_quest_npc_location";
constexpr const char* Medic = "ui_pda2_medic_location";
constexpr const char* ActorBox = "ui_pda2_actor_box_location";
constexpr const char* ActorSleep = "ui_pda2_actor_sleep_location";
} // namespace PdaMapSpot

