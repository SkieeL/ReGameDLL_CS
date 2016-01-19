/*
*
*   This program is free software; you can redistribute it and/or modify it
*   under the terms of the GNU General Public License as published by the
*   Free Software Foundation; either version 2 of the License, or (at
*   your option) any later version.
*
*   This program is distributed in the hope that it will be useful, but
*   WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
*   General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software Foundation,
*   Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*   In addition, as a special exception, the author gives permission to
*   link the code of this program with the Half-Life Game Engine ("HL
*   Engine") and Modified Game Libraries ("MODs") developed by Valve,
*   L.L.C ("Valve").  You must obey the GNU General Public License in all
*   respects for all of the code used other than the HL Engine and MODs
*   from Valve.  If you modify this file, you may extend this exception
*   to your version of the file, but you are not obligated to do so.  If
*   you do not wish to do so, delete this exception statement from your
*   version.
*
*/

#ifndef CS_BOT_MANAGER_H
#define CS_BOT_MANAGER_H
#ifdef _WIN32
#pragma once
#endif

#ifdef HOOK_GAMEDLL

#define TheBots (*pTheBots)

//#define m_flNextCVarCheck (*pm_flNextCVarCheck)
//#define m_isMapDataLoaded (*pm_isMapDataLoaded)
//#define m_editCmd (*pm_editCmd)

//#define m_isLearningMap (*pm_isLearningMap)
//#define m_isAnalysisRequested (*pm_isAnalysisRequested)

#endif // HOOK_GAMEDLL

extern CBotManager *TheBots;

// The manager for Counter-Strike specific bots
class CCSBotManager: public CBotManager
{
public:
	CCSBotManager();

	virtual void ClientDisconnect(CBasePlayer *pPlayer);
	virtual BOOL ClientCommand(CBasePlayer *pPlayer, const char *pcmd);

	virtual void ServerActivate(void);
	virtual void ServerDeactivate(void);

	virtual void ServerCommand(const char *pcmd);
	virtual void AddServerCommand(const char *cmd);
	virtual void AddServerCommands(void);

	virtual void RestartRound(void);									// (EXTEND) invoked when a new round begins
	virtual void StartFrame(void);										// (EXTEND) called each frame

	virtual void OnEvent(GameEventType event, CBaseEntity *entity = NULL, CBaseEntity *other = NULL);
	virtual unsigned int GetPlayerPriority(CBasePlayer *player) const;					// return priority of player (0 = max pri)
	virtual bool IsImportantPlayer(CBasePlayer *player) const;						// return true if player is important to scenario (VIP, bomb carrier, etc)

#ifdef HOOK_GAMEDLL

	void ClientDisconnect_(CBasePlayer *pPlayer);
	BOOL ClientCommand_(CBasePlayer *pPlayer, const char *pcmd);
	void ServerActivate_(void);
	void ServerDeactivate_(void);
	void ServerCommand_(const char *pcmd);
	void AddServerCommand_(const char *cmd);
	void AddServerCommands_(void);
	void RestartRound_(void);
	void StartFrame_(void);
	void OnEvent_(GameEventType event, CBaseEntity *entity, CBaseEntity *other);
	unsigned int GetPlayerPriority_(CBasePlayer *player) const;
	bool IsImportantPlayer_(CBasePlayer *player) const;

#endif // HOOK_GAMEDLL

public:
	void ValidateMapData(void);
	bool IsLearningMap(void) const		{ return IMPL(m_isLearningMap); }
	void SetLearningMapFlag(void)		{ IMPL(m_isLearningMap) = true;}
	bool IsAnalysisRequested(void) const	{ return IMPL(m_isAnalysisRequested); }
	void RequestAnalysis(void)		{ IMPL(m_isAnalysisRequested) = true; }
	void AckAnalysisRequest(void)		{ IMPL(m_isAnalysisRequested) = false; }

	// difficulty levels
	static BotDifficultyType GetDifficultyLevel(void)
	{
		if (cv_bot_difficulty.value < 0.9f)
			return BOT_EASY;

		if (cv_bot_difficulty.value < 1.9f)
			return BOT_NORMAL;

		if (cv_bot_difficulty.value < 2.9f)
			return BOT_HARD;

		return BOT_EXPERT;
	}

	// the supported game scenarios
	enum GameScenarioType
	{
		SCENARIO_DEATHMATCH,
		SCENARIO_DEFUSE_BOMB,
		SCENARIO_RESCUE_HOSTAGES,
		SCENARIO_ESCORT_VIP
	};
	GameScenarioType GetScenario(void) const		{ return m_gameScenario; }

	// "zones"
	// depending on the game mode, these are bomb zones, rescue zones, etc.
	enum { MAX_ZONES = 4 };								// max # of zones in a map
	enum { MAX_ZONE_NAV_AREAS = 16 };						// max # of nav areas in a zone
	struct Zone
	{
		CBaseEntity *m_entity;					// the map entity
		CNavArea *m_area[MAX_ZONE_NAV_AREAS];			// nav areas that overlap this zone
		int m_areaCount;
		Vector m_center;
		bool m_isLegacy;					// if true, use pev->origin and 256 unit radius as zone
		int m_index;
		Extent m_extent;

	};/* size: 116, cachelines: 2, members: 7 */

	const Zone *GetZone(int i) const				{ return &m_zone[i]; }
	const Zone *GetZone(const Vector *pos) const;										// return the zone that contains the given position
	const Zone *GetClosestZone(const Vector *pos) const;									// return the closest zone to the given position
	const Zone *GetClosestZone(const CBaseEntity *entity) const	{ return GetClosestZone(&entity->pev->origin); }	// return the closest zone to the given entity
	int GetZoneCount(void) const					{ return m_zoneCount; }

	const Vector *GetRandomPositionInZone(const Zone *zone) const;
	CNavArea *GetRandomAreaInZone(const Zone *zone) const;

	// Return the zone closest to the given position, using the given cost heuristic
	template<typename CostFunctor>
	const Zone *GetClosestZone(CNavArea *startArea, CostFunctor costFunc, float *travelDistance = NULL) const
	{
		const Zone *closeZone = NULL;
		float closeDist = 99999999.9f;

		if (startArea == NULL)
			return NULL;

		for (int i = 0; i < m_zoneCount; ++i)
		{
			if (m_zone[i].m_areaCount == 0)
				continue;

			if (m_zone[i].m_isBlocked)
				continue;

			// just use the first overlapping nav area as a reasonable approximation
			float dist = NavAreaTravelDistance(startArea, m_zone[i].m_area[0], costFunc);

			if (dist >= 0.0f && dist < closeDist)
			{
				closeZone = &m_zone[i];
				closeDist = dist;
			}
		}

		if (travelDistance != NULL)
			*travelDistance = closeDist;

		return closeZone;
	}

	// pick a zone at random and return it
	const Zone *GetRandomZone(void) const
	{
		if (!m_zoneCount)
			return NULL;

		return &m_zone[ RANDOM_LONG(0, m_zoneCount - 1) ];
	}
	
	bool IsBombPlanted(void) const			{ return m_isBombPlanted; }						// returns true if bomb has been planted
	float GetBombPlantTimestamp(void) const		{ return m_bombPlantTimestamp; }					// return time bomb was planted
	bool IsTimeToPlantBomb(void) const		{ return (gpGlobals->time >= m_earliestBombPlantTimestamp); }		// return true if it's ok to try to plant bomb
	CBasePlayer *GetBombDefuser(void) const		{ return m_bombDefuser; }						// return the player currently defusing the bomb, or NULL
	float GetBombTimeLeft(void) const;											// get the time remaining before the planted bomb explodes
	CBaseEntity *GetLooseBomb(void)			{ return m_looseBomb; }							// return the bomb if it is loose on the ground
	CNavArea *GetLooseBombArea(void) const		{ return m_looseBombArea; }						// return area that bomb is in/near
	void SetLooseBomb(CBaseEntity *bomb);

	float GetRadioMessageTimestamp(GameEventType event, int teamID) const;			// return the last time the given radio message was sent for given team
	float GetRadioMessageInterval(GameEventType event, int teamID) const;			// return the interval since the last time this message was sent
	void SetRadioMessageTimestamp(GameEventType event, int teamID);
	void ResetRadioMessageTimestamps(void);

	float GetLastSeenEnemyTimestamp(void) const	{ return m_lastSeenEnemyTimestamp; }				// return the last time anyone has seen an enemy
	void SetLastSeenEnemyTimestamp(void)		{ m_lastSeenEnemyTimestamp = gpGlobals->time; }

	float GetRoundStartTime(void) const		{ return m_roundStartTimestamp; }
	float GetElapsedRoundTime(void) const		{ return gpGlobals->time - m_roundStartTimestamp; }		// return the elapsed time since the current round began

	bool AllowRogues(void) const			{ return cv_bot_allow_rogues.value != 0.0f; }
	bool AllowPistols(void) const			{ return cv_bot_allow_pistols.value != 0.0f; }
	bool AllowShotguns(void) const			{ return cv_bot_allow_shotguns.value != 0.0f; }
	bool AllowSubMachineGuns(void) const		{ return cv_bot_allow_sub_machine_guns.value != 0.0f; }
	bool AllowRifles(void) const			{ return cv_bot_allow_rifles.value != 0.0f; }
	bool AllowMachineGuns(void) const		{ return cv_bot_allow_machine_guns.value != 0.0f; }
	bool AllowGrenades(void) const			{ return cv_bot_allow_grenades.value != 0.0f; }
	bool AllowSnipers(void) const			{ return cv_bot_allow_snipers.value != 0.0f; }
	bool AllowTacticalShield(void) const		{ return cv_bot_allow_shield.value != 0.0f; }
	bool AllowFriendlyFireDamage(void) const	{ return friendlyfire.value != 0.0f; }

	bool IsWeaponUseable(CBasePlayerItem *item) const;						// return true if the bot can use this weapon

	bool IsDefenseRushing(void) const		{ return m_isDefenseRushing; }			// returns true if defense team has "decided" to rush this round
	bool IsOnDefense(CBasePlayer *player) const;							// return true if this player is on "defense"
	bool IsOnOffense(CBasePlayer *player) const;							// return true if this player is on "offense"

	bool IsRoundOver(void) const			{ return m_isRoundOver; }			// return true if the round has ended

	unsigned int GetNavPlace(void) const		{ return m_navPlace; }
	void SetNavPlace(unsigned int place)		{ m_navPlace = place; }

	enum SkillType { LOW, AVERAGE, HIGH, RANDOM };
	NOXREF NOBODY const char *GetRandomBotName(SkillType skill);

	static void MonitorBotCVars(void);
	static void MaintainBotQuota(void);
	NOBODY static bool AddBot(const BotProfile *profile, BotProfileTeamType team);

	#define FROM_CONSOLE true
	static bool BotAddCommand(BotProfileTeamType team, bool isFromConsole = false);			// process the "bot_add" console command

#ifndef HOOK_GAMEDLL
private:
#endif // HOOK_GAMEDLL
	static float IMPL(m_flNextCVarCheck);
	static bool IMPL(m_isMapDataLoaded);				// true if we've attempted to load map data
	static bool IMPL(m_isLearningMap);
	static bool IMPL(m_isAnalysisRequested);

	GameScenarioType m_gameScenario;				// what kind of game are we playing

	Zone m_zone[ MAX_ZONES ];
	int m_zoneCount;

	bool m_isBombPlanted;						// true if bomb has been planted
	float m_bombPlantTimestamp;					// time bomb was planted
	float m_earliestBombPlantTimestamp;				// don't allow planting until after this time has elapsed
	CBasePlayer *m_bombDefuser;					// the player currently defusing a bomb
	EHANDLE m_looseBomb;						// will be non-NULL if bomb is loose on the ground
	CNavArea *m_looseBombArea;					// area that bomb is is/near

	bool m_isRoundOver;						// true if the round has ended

	float m_radioMsgTimestamp[24][2];

	float m_lastSeenEnemyTimestamp;
	float m_roundStartTimestamp;					// the time when the current round began

	bool m_isDefenseRushing;					// whether defensive team is rushing this round or not

	static NavEditCmdType IMPL(m_editCmd);
	unsigned int m_navPlace;
	CountdownTimer m_respawnTimer;
	bool m_isRespawnStarted;
	bool m_canRespawn;
	bool m_bServerActive;

};/* size: 736, cachelines: 12, members: 25 */

/* <2e81a8> ../cstrike/dlls/bot/cs_bot_manager.h:24 */
NOXREF inline int OtherTeam(int team)
{
	return (team == TERRORIST) ? CT : TERRORIST;
}

/* <111bd2> ../cstrike/dlls/bot/cs_bot_manager.h:266 */
inline CCSBotManager *TheCSBots(void)
{
	return reinterpret_cast<CCSBotManager *>(TheBots);
}

void PrintAllEntities(void);
void UTIL_DrawBox(Extent *extent, int lifetime, int red, int green, int blue);

#ifdef HOOK_GAMEDLL

typedef const CCSBotManager::Zone *(CCSBotManager::*GET_ZONE_INT)(int) const;
typedef const CCSBotManager::Zone *(CCSBotManager::*GET_ZONE_VECTOR)(const Vector *pos) const;

typedef const CCSBotManager::Zone *(CCSBotManager::*GET_CLOSEST_ZONE_ENT)(const CBaseEntity *entity) const;
typedef const CCSBotManager::Zone *(CCSBotManager::*GET_CLOSEST_ZONE_VECTOR)(const Vector *pos) const;

#endif // HOOK_GAMEDLL

// refs
extern void (*pCCSBotManager__AddBot)(void);

#endif // CS_BOT_MANAGER_H
