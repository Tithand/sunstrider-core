
#ifndef TRINITY_MAP_H
#define TRINITY_MAP_H

#include "Define.h"
#include "GridDefines.h"
#include "Cell.h"
#include "GridRefManager.h"
#include "MapRefManager.h"
#include "MersenneTwister.h"
#include "DynamicTree.h"
#include "Models/GameObjectModel.h"

#include <bitset>
#include <list>
#include <mutex>

class Unit;
class WorldPacket;
class InstanceScript;
class WorldObject;
class CreatureGroup;
class Battleground;
class GridMap;
class Transport;
class MotionTransport;
namespace Trinity { struct ObjectUpdater; }
struct MapDifficulty;
struct MapEntry;
enum Difficulty : int;
class BattlegroundMap;
class InstanceMap;
class MapInstanced;
enum WeatherState : int;

struct ObjectMover
{
    ObjectMover() : x(0), y(0), z(0), ang(0) {}
    ObjectMover(float _x, float _y, float _z, float _ang) : x(_x), y(_y), z(_z), ang(_ang) {}

    float x, y, z, ang;
};

// GCC have alternative #pragma pack(N) syntax and old gcc version not support pack(push,N), also any gcc version not support it at some platform
#if defined( __GNUC__ )
#pragma pack(1)
#else
#pragma pack(push,1)
#endif

struct InstanceTemplate
{
    uint32 parent;    
    uint32 maxPlayers;
    uint32 reset_delay;
    uint32 access_id;
    float startLocX;
    float startLocY;
    float startLocZ;
    float startLocO;
    uint32 ScriptId;
    bool heroicForced = false;
};

struct InstanceTemplateAddon
{
    uint32 map;
    bool forceHeroicEnabled; //true to enable this entry
};

enum LevelRequirementVsMode
{
    LEVELREQUIREMENT_HEROIC = 70
};

struct ZoneDynamicInfo
{
    ZoneDynamicInfo();

    uint32 MusicId;
    WeatherState WeatherId;
    float WeatherGrade;
    uint32 OverrideLightId;
    uint32 LightFadeInTime;
};

#if defined( __GNUC__ )
#pragma pack()
#else
#pragma pack(pop)
#endif

typedef std::unordered_map<Creature*, ObjectMover> CreatureMoveList;
typedef std::unordered_map<GameObject*, ObjectMover> GameObjectMoveList;

typedef std::map<uint32/*leaderDBGUID*/, CreatureGroup*>        CreatureGroupHolderType;
typedef std::unordered_map<uint32 /*zoneId*/, ZoneDynamicInfo> ZoneDynamicInfoMap;

enum MapType
{
    //not specialized
    MAP_TYPE_MAP, 
    //MapInstanced class
    MAP_TYPE_MAP_INSTANCED,
    //InstanceMap class
    MAP_TYPE_INSTANCE_MAP,
    //BattlegroundMap class
    MAP_TYPE_BATTLEGROUND_MAP,
};

class TC_GAME_API Map : public GridRefManager<NGridType>
{
    friend class MapReference;
    public:
        Map(MapType type, uint32 id, uint32 InstanceId, uint8 SpawnMode);
        ~Map() override;

        MapEntry const* GetEntry() const { return i_mapEntry; }
        MapType GetMapType() const { return i_mapType; }

        // currently unused for normal maps
        bool CanUnload(uint32 diff)
        {
            if(!m_unloadTimer) return false;
            if(m_unloadTimer <= diff) return true;
            m_unloadTimer -= diff;
            return false;
        }

        virtual bool Add(Player *);
        virtual void Remove(Player *, bool);
        template<class T> bool Add(T *, bool checkTransport = false);
        template<class T> void Remove(T *, bool);

        void VisitNearbyCellsOf(WorldObject* obj, TypeContainerVisitor<Trinity::ObjectUpdater, GridTypeMapContainer> &gridVisitor, TypeContainerVisitor<Trinity::ObjectUpdater, WorldTypeMapContainer> &worldVisitor);
        //this wrap map udpates and call it with diff since last updates. If minimumTimeSinceLastUpdate, the thread will sleep until minimumTimeSinceLastUpdate is reached
        void DoUpdate(uint32 maxDiff, uint32 minimumTimeSinceLastUpdate = 0);
        virtual void Update(const uint32&);

        void MessageBroadcast(Player*, WorldPacket *, bool to_self, bool to_possessor);
        void MessageBroadcast(WorldObject *, WorldPacket *, bool to_possessor);
        void MessageDistBroadcast(Player *, WorldPacket *, float dist, bool to_self, bool to_possessor, bool own_team_only = false);
        void MessageDistBroadcast(WorldObject *, WorldPacket *, float dist, bool to_possessor);

		virtual float GetDefaultVisibilityDistance() const;
        float GetVisibilityRange() const { return m_VisibleDistance; }
        //function for setting up visibility distance for maps on per-type/per-Id basis
        virtual void InitVisibilityDistance();
		void SetVisibilityDistance(float dist);

        void PlayerRelocation(Player* player, float x, float y, float z, float angle);
        void CreatureRelocation(Creature* creature, float x, float y, float z, float angle);
        void GameObjectRelocation(GameObject* gob, float x, float y, float z, float angle);
        //void DynamicObjectRelocation(DynamicObject* dob, float x, float y, float z, float angle);

        template<class T, class CONTAINER> void Visit(const Cell &cell, TypeContainerVisitor<T, CONTAINER> &visitor);

        void LoadGrid(float x, float y);
        bool UnloadGrid(const uint32 &x, const uint32 &y, bool pForce);
        virtual void UnloadAll();

        bool IsGridLoaded(float x, float y) const;

        uint32 GetId(void) const { return i_id; }

        void LoadMapAndVMap(uint32 mapid, uint32 instanceid, int x, int y);

        // some calls like isInWater should not use vmaps due to processor power
        // can return INVALID_HEIGHT if under z+2 z coord not found height
        float _GetHeight(float x, float y, float z, bool vmap = true, float maxSearchDist = DEFAULT_HEIGHT_SEARCH) const;
        /* Returns closest height for given position, search in map height.
        @checkVMap search in vmap height as well. If both map and vmap heights were found, the closest one will be returned
        walkableOnly NYI
        Returns INVALID_HEIGHT if no height found at position or if height is further than maxSearchDist
        */
        float GetHeight(float x, float y, float z, bool vmap = true, float maxSearchDist = DEFAULT_HEIGHT_SEARCH, bool walkableOnly = false) const;
        float GetMinHeight(float x, float y) const;
        /* Get map level (checking vmaps) or liquid level at given point */
        float GetWaterOrGroundLevel(float x, float y, float z, float* ground = nullptr, bool swim = false) const;
        //Returns INVALID_HEIGHT if nothing found. walkableOnly NYI
        float GetHeight(PhaseMask phasemask, float x, float y, float z, bool vmap = true, float maxSearchDist = DEFAULT_HEIGHT_SEARCH, bool walkableOnly = false) const;
        bool GetLiquidLevelBelow(float x, float y, float z, float& liquidLevel, float maxSearchDist = DEFAULT_HEIGHT_SEARCH) const;
        void RemoveGameObjectModel(const GameObjectModel& model) { _dynamicTree.remove(model); }
        void InsertGameObjectModel(const GameObjectModel& model) { _dynamicTree.insert(model); }
        bool ContainsGameObjectModel(const GameObjectModel& model) const { return _dynamicTree.contains(model);}
        Transport* GetTransportForPos(uint32 phase, float x, float y, float z, WorldObject* worldobject = nullptr);

        bool isInLineOfSight(float x1, float y1, float z1, float x2, float y2, float z2, PhaseMask phasemask = (PhaseMask)0) const;
        void Balance() { _dynamicTree.balance(); }
        //get dynamic collision (gameobjects only ?)
        bool getObjectHitPos(PhaseMask phasemask, float x1, float y1, float z1, float x2, float y2, float z2, float& rx, float &ry, float& rz, float modifyDist);

        ZLiquidStatus getLiquidStatus(float x, float y, float z, BaseLiquidTypeMask reqBaseLiquidTypeMask, LiquidData *data = nullptr) const;

        uint32 GetAreaId(float x, float y, float z, bool *isOutdoors) const;
        bool GetAreaInfo(float x, float y, float z, uint32 &mogpflags, int32 &adtId, int32 &rootId, int32 &groupId) const;

        bool IsOutdoors(float x, float y, float z) const;

        uint8 GetTerrainType(float x, float y) const;
        float GetWaterLevel(float x, float y) const;
        //IsUnderWater is implied by this
        bool IsInWater(float x, float y, float z, LiquidData *data = nullptr) const;
        bool IsUnderWater(float x, float y, float z) const;

        uint32 GetAreaId(float x, float y, float z) const;
        uint32 GetZoneId(float x, float y, float z) const;
        void GetZoneAndAreaId(uint32& zoneid, uint32& areaid, float x, float y, float z) const;

        virtual void MoveAllCreaturesInMoveList();
        virtual void MoveAllGameObjectsInMoveList();
        virtual void RemoveAllObjectsInRemoveList();
        virtual void RemoveAllPlayers();

        bool CreatureRespawnRelocation(Creature *c, bool diffGridOnly);        // used only in MoveAllCreaturesInMoveList and ObjectGridUnloader
        bool GameObjectRespawnRelocation(GameObject* go, bool diffGridOnly);

        // assert print helper
        bool CheckGridIntegrity(Creature* c, bool moved) const;
        bool CheckGridIntegrity(GameObject* c, bool moved) const;

        uint32 GetInstanceId() const { return i_InstanceId; }
        uint8 GetSpawnMode() const { return (i_spawnMode); }
        virtual bool CanEnter(Player* /*player*/) { return true; }
        const char* GetMapName() const;

        // have meaning only for instanced map (that have set real difficulty)
        Difficulty GetDifficulty() const;
        bool IsRegularDifficulty() const;
        MapDifficulty const* GetMapDifficulty() const;

        bool Instanceable() const;
        // NOTE: this duplicate of Instanceable(), but Instanceable() can be changed when BG also will be instanceable
        bool IsDungeon() const;
        bool IsNonRaidDungeon() const;
        bool IsRaid() const;
        bool IsWorldMap() const;
        bool IsHeroic() const;

        bool IsBattleground() const;
        bool IsBattleArena() const;
        bool IsBattlegroundOrArena() const;
   
        void AddObjectToRemoveList(WorldObject *obj);
        void AddObjectToSwitchList(WorldObject *obj, bool on);
        virtual void DelayedUpdate(const uint32 diff);

        virtual bool RemoveBones(uint64 guid, float x, float y);

        void UpdateObjectVisibility(WorldObject* obj, Cell cell, CellCoord cellpair);

        void resetMarkedCells() { marked_cells.reset(); }
        bool isCellMarked(uint32 pCellId) { return marked_cells.test(pCellId); }
        void markCell(uint32 pCellId) { marked_cells.set(pCellId); }
        Player* GetPlayer(uint64 guid);
        Creature* GetCreature(uint64 guid);
        GameObject* GetGameObject(uint64 guid);
        Transport* GetTransport(uint64 guid);
        DynamicObject* GetDynamicObject(uint64 guid);  

        //avoid using as much as possible, this locks HashMapHolder
        Creature* GetCreatureWithTableGUID(uint32 tableGUID) const;

        MapInstanced* ToMapInstanced() { if (Instanceable())  return reinterpret_cast<MapInstanced*>(this); else return nullptr; }
        const MapInstanced* ToMapInstanced() const { if (Instanceable())  return (const MapInstanced*)((MapInstanced*)this); else return nullptr; }

        InstanceMap* ToInstanceMap() { if (IsDungeon())  return reinterpret_cast<InstanceMap*>(this); else return nullptr; }
        const InstanceMap* ToInstanceMap() const { if (IsDungeon())  return (const InstanceMap*)((InstanceMap*)this); else return nullptr; }

        BattlegroundMap* ToBattlegroundMap() { if (IsBattlegroundOrArena()) return reinterpret_cast<BattlegroundMap*>(this); else return nullptr; }
        const BattlegroundMap* ToBattlegroundMap() const { if (IsBattlegroundOrArena()) return reinterpret_cast<BattlegroundMap const*>(this); return nullptr; }


        bool HavePlayers() const { return !m_mapRefManager.isEmpty(); }
        uint32 GetPlayersCountExceptGMs() const;
        bool ActiveObjectsNearGrid(uint32 x, uint32 y) const;

        void AddUnitToNotify(Unit* unit);
        void RelocationNotify();

        void SendToPlayers(WorldPacket* data) const;

        typedef MapRefManager PlayerList;
        PlayerList const& GetPlayers() const { return m_mapRefManager; }

        // must called with AddToWorld
        template<class T>
        void AddToForceActive(T* obj) { AddToForceActiveHelper(obj); }

        void AddToForceActive(Creature* obj);

        // must called with RemoveFromWorld
        template<class T>
        void RemoveFromForceActive(T* obj) { RemoveFromForceActiveHelper(obj); }

        void RemoveFromForceActive(Creature* obj);

        template<class T> void SwitchGridContainers(T* obj, bool active);
        template<class NOTIFIER> void VisitAll(const float &x, const float &y, float radius, NOTIFIER &notifier);
        template<class NOTIFIER> void VisitWorld(const float &x, const float &y, float radius, NOTIFIER &notifier);
        template<class NOTIFIER> void VisitGrid(const float &x, const float &y, float radius, NOTIFIER &notifier);
        CreatureGroupHolderType CreatureGroupHolder;
        MTRand mtRand;

        int32 irand(int32 min, int32 max)
        {
          return int32 (mtRand.randInt(max - min)) + min;
        }

        uint32 urand(uint32 min, uint32 max)
        {
          return mtRand.randInt(max - min) + min;
        }

        int32 rand32()
        {
          return mtRand.randInt();
        }

        double rand_norm(void)
        {
          return mtRand.randExc();
        }

        double rand_chance(void)
        {
          return mtRand.randExc(100.0);
        }
        
        void AddCreatureToPool(Creature*, uint32);
        void RemoveCreatureFromPool(Creature*, uint32);
        std::list<Creature*> GetAllCreaturesFromPool(uint32);

        // Objects that must update even in inactive grids without activating them
        typedef std::set<MotionTransport*> TransportsContainer;
        TransportsContainer _transports;
        TransportsContainer::iterator _transportsUpdateIter;

        //this function is overrided by InstanceMap and BattlegroundMap to handle crash recovery
        virtual void HandleCrash() { ASSERT(false); }

        void SetZoneMusic(uint32 zoneId, uint32 musicId);
        void SetZoneWeather(uint32 zoneId, WeatherState weatherId, float weatherGrade);
        void SetZoneOverrideLight(uint32 zoneId, uint32 lightId, uint32 fadeInTime);

		uint32 GetLastMapUpdateTime() const { return _lastMapUpdate; }
    private:

        void LoadVMap(int pX, int pY);
        void LoadMap(uint32 mapid, uint32 instanceid, int x,int y);

        GridMap *GetGrid(float x, float y);

        //uint64 CalculateGridMask(const uint32 &y) const;

        void SendInitSelf( Player * player );

        void SendInitTransports( Player * player );
        void SendRemoveTransports( Player * player );

        void SendZoneDynamicInfo(Player* player);

        bool CreatureCellRelocation(Creature* creature, Cell new_cell);
        bool GameObjectCellRelocation(GameObject* gob, Cell new_cell);

        void AddCreatureToMoveList(Creature* c, float x, float y, float z, float ang);
        void AddGameObjectToMoveList(GameObject* go, float x, float y, float z, float ang);
        CreatureMoveList i_creaturesToMove;
        GameObjectMoveList i_gameObjectsToMove;

        bool loaded(const GridPair &) const;
        void EnsureGridLoaded(const Cell&, Player* player = nullptr);
        void EnsureGridCreated(const GridPair &);
        void EnsureGridCreated_i(const GridPair &);

        void buildNGridLinkage(NGridType* pNGridType) { pNGridType->link(this); }

        template<class T> void AddType(T *obj);
        template<class T> void RemoveType(T *obj, bool);

        NGridType* getNGrid(uint32 x, uint32 y) const
        {
            assert(x < MAX_NUMBER_OF_GRIDS);
            assert(y < MAX_NUMBER_OF_GRIDS);
            return i_grids[x][y];
        }

        bool isGridObjectDataLoaded(uint32 x, uint32 y) const { return getNGrid(x,y)->isGridObjectDataLoaded(); }
        void setGridObjectDataLoaded(bool pLoaded, uint32 x, uint32 y) { getNGrid(x,y)->setGridObjectDataLoaded(pLoaded); }

        void setNGrid(NGridType* grid, uint32 x, uint32 y);

        void UpdateActiveCells(const float &x, const float &y, const uint32 &t_diff);

        bool AllTransportsEmpty() const; // pussywizard
        void AllTransportsRemovePassengers(); // pussywizard
        TransportsContainer const& GetAllTransports() const { return _transports; }

    protected:
        std::mutex _mapLock;
        std::mutex _gridLock;

        MapEntry const* i_mapEntry;
        uint8 i_spawnMode;
        uint32 i_id;
        uint32 i_InstanceId;
        uint32 m_unloadTimer;
        float m_VisibleDistance;
        DynamicMapTree _dynamicTree;

        MapRefManager m_mapRefManager;
        MapRefManager::iterator m_mapRefIter;

        /** The objects in m_activeForcedNonPlayers are always kept active and makes everything around them also active, just like players
        */
        typedef std::set<WorldObject*> ActiveForcedNonPlayers;
        ActiveForcedNonPlayers m_activeForcedNonPlayers;
        ActiveForcedNonPlayers::iterator m_activeForcedNonPlayersIter;

    private:
        NGridType* i_grids[MAX_NUMBER_OF_GRIDS][MAX_NUMBER_OF_GRIDS];
        GridMap *GridMaps[MAX_NUMBER_OF_GRIDS][MAX_NUMBER_OF_GRIDS];
        std::bitset<TOTAL_NUMBER_OF_CELLS_PER_MAP*TOTAL_NUMBER_OF_CELLS_PER_MAP> marked_cells;

        MapType i_mapType;
        bool i_lock;
        std::vector<uint64> i_unitsToNotifyBacklog;
        std::vector<Unit*> i_unitsToNotify;
        std::set<WorldObject *> i_objectsToRemove;
        std::map<WorldObject*, bool> i_objectsToSwitch;

        // Type specific code for add/remove to/from grid
        template<class T>
            void AddToGrid(T*, NGridType *, Cell const&);

        template<class T>
            void AddNotifier(T*);

        template<class T>
            void RemoveFromGrid(T*, NGridType *, Cell const&);

        template<class T>
            void DeleteFromWorld(T*);

        template<class T>
        void AddToForceActiveHelper(T* obj)
        {
            m_activeForcedNonPlayers.insert(obj);
        }

        template<class T>
        void RemoveFromForceActiveHelper(T* obj)
        {
            // Map::Update for active object in proccess
            if(m_activeForcedNonPlayersIter != m_activeForcedNonPlayers.end())
            {
                auto itr = m_activeForcedNonPlayers.find(obj);
                if(itr == m_activeForcedNonPlayers.end())
                    return;
                if(itr==m_activeForcedNonPlayersIter)
                    ++m_activeForcedNonPlayersIter;
                m_activeForcedNonPlayers.erase(itr);
            }
            else
                m_activeForcedNonPlayers.erase(obj);
        }

        typedef std::map<uint32, std::set<uint64> > CreaturePoolMember;
        CreaturePoolMember m_cpmembers;

        ZoneDynamicInfoMap _zoneDynamicInfo;
        uint32 _defaultLight;

        uint32 _lastMapUpdate;
};

enum InstanceResetMethod
{
    INSTANCE_RESET_ALL,
    INSTANCE_RESET_CHANGE_DIFFICULTY,
    INSTANCE_RESET_GLOBAL,
    INSTANCE_RESET_GROUP_DISBAND,
    INSTANCE_RESET_GROUP_JOIN,
    INSTANCE_RESET_RESPAWN_DELAY
};

class TC_GAME_API InstanceMap : public Map
{
    public:
        InstanceMap(uint32 id, uint32 InstanceId, uint8 SpawnMode);
        ~InstanceMap() override;
        bool Add(Player *) override;
        void Remove(Player *, bool) override;
        void Update(const uint32&) override;
        void CreateInstanceScript(bool load);
        bool Reset(uint8 method);
        uint32 GetScriptId() { return i_script_id; }
        InstanceScript* GetInstanceScript() { return i_data; }
        void PermBindAllPlayers(Player *player);
        void UnloadAll() override;
        void HandleCrash() override;
        bool CanEnter(Player* player) override;
        void SendResetWarnings(uint32 timeLeft) const;
        void SetResetSchedule(bool on);

        float GetDefaultVisibilityDistance() const override;
    private:
        bool m_resetAfterUnload;
        bool m_unloadWhenEmpty;
        InstanceScript* i_data;
        uint32 i_script_id;
};

class TC_GAME_API BattlegroundMap : public Map
{
    public:
        BattlegroundMap(uint32 id, uint32 InstanceId);
        ~BattlegroundMap() override;

        bool Add(Player *) override;
        void Remove(Player *, bool) override;
        bool CanEnter(Player* player) override;
        void SetUnload();
        //void UnloadAll();
        void RemoveAllPlayers() override;

        void HandleCrash() override;

		float GetDefaultVisibilityDistance() const override;
        Battleground* GetBG() { return m_bg; }
        void SetBG(Battleground* bg) { m_bg = bg; }
    private:
        Battleground* m_bg;
};

/*inline
uint64
Map::CalculateGridMask(const uint32 &y) const
{
    uint64 mask = 1;
    mask <<= y;
    return mask;
}
*/

template<class T, class CONTAINER>
inline void
Map::Visit(const Cell &cell, TypeContainerVisitor<T, CONTAINER> &visitor)
{
    const uint32 x = cell.GridX();
    const uint32 y = cell.GridY();
    const uint32 cell_x = cell.CellX();
    const uint32 cell_y = cell.CellY();

    if( !cell.NoCreate() || loaded(GridPair(x,y)) )
    {
        EnsureGridLoaded(cell);
        //LOCK_TYPE guard(i_info[x][y]->i_lock);
        getNGrid(x, y)->Visit(cell_x, cell_y, visitor);
    }
}

template<class NOTIFIER>
inline void
Map::VisitAll(const float &x, const float &y, float radius, NOTIFIER &notifier)
{
    CellCoord p(Trinity::ComputeCellCoord(x, y));
    Cell cell(p);
    cell.data.Part.reserved = ALL_DISTRICT;
    cell.SetNoCreate();

    TypeContainerVisitor<NOTIFIER, WorldTypeMapContainer> world_object_notifier(notifier);
    cell.Visit(p, world_object_notifier, *this, radius, x, y);
    TypeContainerVisitor<NOTIFIER, GridTypeMapContainer >  grid_object_notifier(notifier);
    cell.Visit(p, grid_object_notifier, *this, radius, x, y);
}

template<class NOTIFIER>
inline void
Map::VisitWorld(const float &x, const float &y, float radius, NOTIFIER &notifier)
{
    CellCoord p(Trinity::ComputeCellCoord(x, y));
    Cell cell(p);
    cell.data.Part.reserved = ALL_DISTRICT;
    cell.SetNoCreate();

    TypeContainerVisitor<NOTIFIER, WorldTypeMapContainer> world_object_notifier(notifier);
    cell.Visit(p, world_object_notifier, *this, radius, x, y);
}

template<class NOTIFIER>
inline void
Map::VisitGrid(const float &x, const float &y, float radius, NOTIFIER &notifier)
{
    CellCoord p(Trinity::ComputeCellCoord(x, y));
    Cell cell(p);
    cell.data.Part.reserved = ALL_DISTRICT;
    cell.SetNoCreate();

    TypeContainerVisitor<NOTIFIER, GridTypeMapContainer >  grid_object_notifier(notifier);
    cell.Visit(p, grid_object_notifier, *this, radius, x, y);
}
#endif
