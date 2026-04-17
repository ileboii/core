#include <random>
#pragma once
#pragma once
#define DT_POLYREF64 1

#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "World.h"
#include "PathFinder.h"
#include "MapManager.h"
#include "VmangosCompat.h"

class ByteBuffer;

namespace G3D
{
    class Vector2;
    class Vector3;
    class Vector4;
}

namespace ai
{
    //Constructor types for WorldPosition
    enum WorldPositionConst
    {
        WP_RANDOM = 0,
        WP_CENTROID = 1,
        WP_MEAN_CENTROID = 2,
        WP_CLOSEST = 3
    };

    template <class D, class W, class URBG>
    inline void WeightedShuffle
    (D first, D last
        , W first_weight, W last_weight
        , URBG&& g)
    {
        while (first != last && first_weight != last_weight)
        {
            std::discrete_distribution<int> dd(first_weight, last_weight);
            auto i = dd(g);

            if (i)
            {
                std::swap(*first, *std::next(first, i));
                std::swap(*first_weight, *std::next(first_weight, i));
            }
            ++first;
            ++first_weight;
        }
    }

    class GuidPosition;

    typedef std::pair<int, int> mGridPair;

    //Extension of WorldLocation with distance functions.
    class WorldPosition : public WorldLocation
    {
    public:
        //Constructors
        void add();
        void rem();
        WorldPosition() : WorldLocation(0,0,0,0,0) { add(); };
        WorldPosition(const WorldLocation& loc) : WorldLocation(loc) { add(); }
        WorldPosition(const WorldPosition& pos) : WorldLocation(pos) { add(); }
        WorldPosition(const std::string str) {char p; std::stringstream  out(str); out >> mapId >> p >> x >> p >> y >> p >> z >> p >> o; add();}
        WorldPosition(const uint32 mapId, const float x, const float y, const float z = 0, float o = 0) : WorldLocation(mapId, x, y, z, o) { add(); }
        WorldPosition(const uint32 mapId, const Position& pos) : WorldLocation(mapId, pos.x, pos.y, pos.z, pos.o) { add(); }
        WorldPosition(const WorldObject* wo) { if (wo) { set(WorldLocation(wo->GetMapId(), wo->GetPositionX(), wo->GetPositionY(), wo->GetPositionZ(), wo->GetOrientation())); } add();}
        WorldPosition(const CreatureDataPair* cdPair) { if (cdPair) { set(WorldLocation(cdPair->second.position.mapId, cdPair->second.position.x, cdPair->second.position.y, cdPair->second.position.z, cdPair->second.position.o)); } add();}
        WorldPosition(const GameObjectDataPair* cdPair) { if (cdPair) { set(WorldLocation(cdPair->second.position.mapId, cdPair->second.position.x, cdPair->second.position.y, cdPair->second.position.z, cdPair->second.position.o)); } add();}
        WorldPosition(const uint32 mapId, const GuidPosition& guidP, uint32 instanceId);
        WorldPosition(const std::vector<WorldPosition*>& list, const WorldPositionConst conType);
        WorldPosition(const std::vector<WorldPosition>& list, const WorldPositionConst conType);
        WorldPosition(const uint32 mapId, const GridPair grid) : WorldLocation(mapId, (int32(grid.x_coord) - CENTER_GRID_ID - 0.5)* SIZE_OF_GRIDS + CENTER_GRID_OFFSET, (int32(grid.y_coord) - CENTER_GRID_ID - 0.5)* SIZE_OF_GRIDS + CENTER_GRID_OFFSET, 0, 0) { add(); }
        WorldPosition(const uint32 mapId, const CellPair cell) : WorldLocation(mapId, (int32(cell.x_coord) - CENTER_GRID_CELL_ID - 0.5)* SIZE_OF_GRID_CELL + CENTER_GRID_CELL_OFFSET, (int32(cell.y_coord) - CENTER_GRID_CELL_ID - 0.5)* SIZE_OF_GRID_CELL + CENTER_GRID_CELL_OFFSET, 0, 0) { add(); }
        WorldPosition(const uint32 mapId, const mGridPair grid) : WorldLocation(mapId, (32 - grid.first)* SIZE_OF_GRIDS, (32 - grid.second)* SIZE_OF_GRIDS, 0, 0) { add(); }
        WorldPosition(const SpellTargetPosition* pos) : WorldLocation(pos->mapId, pos->x, pos->y, pos->z) { add(); }
        WorldPosition(const TaxiNodesEntry* pos) : WorldLocation(pos->map_id, pos->x, pos->y, pos->z) { add(); }
        WorldPosition(const WorldSafeLocsEntry* pos) : WorldLocation(pos->map_id, pos->x, pos->y, pos->z) { add(); }
        WorldPosition(const PlayerInfo* pos) : WorldLocation(pos->mapId, pos->positionX, pos->positionY, pos->positionZ, pos->orientation) { add(); }
        
        virtual ~WorldPosition()
        {
            rem();
        }

        //Setters
        void set(const WorldLocation& pos) { mapId = pos.mapId; x = pos.x; y = pos.y; z = pos.z; o = pos.o; }
        void set(const WorldPosition& pos) { mapId = pos.mapId; x = pos.x; y = pos.y; z = pos.z; o = pos.o; }
        void set(const WorldObject* wo) { set(WorldLocation(wo->GetMapId(), wo->GetPositionX(), wo->GetPositionY(), wo->GetPositionZ(), wo->GetOrientation())); }
        void set(const ObjectGuid& guid, const uint32 mapId, const uint32 instanceId);
        void setMapId(const uint32 id) { mapId = id; }
        void setX(const float val) { x = val; }
        void setY(const float val) { y = val; }
        void setZ(const float val) { z = val; }
        void setO(const float val) { o = val; }

        //Operators
        operator bool() const { return  x != 0 || y != 0 || z != 0; }
        bool operator==(const WorldPosition& p1) const { return mapId == p1.mapId && x == p1.x && y == p1.y && z == p1.z && o == p1.o; }
        bool operator!=(const WorldPosition& p1) const { return mapId != p1.mapId || x != p1.x || y != p1.y || z != p1.z || o != p1.o; }
        
        WorldPosition& operator+=(const WorldPosition& p1) { x += p1.x; y += p1.y; z += p1.z; return *this; }
        WorldPosition& operator-=(const WorldPosition& p1) { x -= p1.x; y -= p1.y; z -= p1.z; return *this; }

        WorldPosition& operator*=(const float s) { x *= s; y *= s; z *= s; return *this; }
        WorldPosition& operator/=(const float s) { x /= s; y /= s; z /= s; return *this; }

        WorldPosition operator+(const WorldPosition& p1) const { WorldPosition p(*this); p += p1; return p; }
        WorldPosition operator-(const WorldPosition& p1) const { WorldPosition p(*this); p -= p1; return p; }

        WorldPosition operator*(const float s) const { WorldPosition p(*this); p *= s; return p; }
        WorldPosition operator/(const float s) const { WorldPosition p(*this); p /= s; return p; }

        float operator*(const WorldPosition& p1) const { return (x * x) + (y * y) + (z * z); }

        //Getters
        uint32 getMapId() const { return mapId; }
        float getX() const { return x; }
        float getY() const { return y; }
        float getZ() const { return z; }
        float getO() const { return o; }
        G3D::Vector3 getVector3() const;
        virtual std::string print() const;
        virtual std::string to_string() const { char p = '|'; std::stringstream out; out << mapId << p << x << p << y << p << z << p << o; return out.str(); };

        static void printWKT(const std::vector<WorldPosition>& points, std::ostringstream& out, const uint32 dim = 0, const bool loop = false);
        void printWKT(std::ostringstream& out) const { printWKT({ *this }, out); }

        bool isOverworld() const { return mapId == 0 || mapId == 1 || mapId == 530 || mapId == 571 || mapId == 609; }
        bool isBg() const { return mapId == 30 || mapId == 489 || mapId == 529 || mapId == 566 || mapId == 607 || mapId == 628; }
        bool isArena() const { return mapId == 559 || mapId == 572 || mapId == 562 || mapId == 617 || mapId == 618; }
        bool isInstance() const { return !isOverworld() || mapId == 609;}
        bool isInWater() const { return getTerrain() ? getTerrain()->IsInWater(x, y, z) : false; };
        bool isUnderWater() const { return getTerrain() ? getTerrain()->IsUnderWater(x, y, z) : false; };
        bool setAtWaterSurface();
        bool isUnderground() const;
        float getWaterLevel() const { return getTerrain() ? getTerrain()->GetWaterLevel(x, y, z) : -200000.0f; };
        float getGroundLevel() const { float ground = 0.0f; getTerrain()->GetWaterLevel(x, y, z, &ground); return ground; };

        WorldPosition relPoint(const WorldPosition& center) const { return WorldPosition(mapId, x - center.x, y - center.y, z - center.z, o); }
        WorldPosition offset(const WorldPosition& center) const { return WorldPosition(mapId, x + center.x, y + center.y, z + center.z, o); }
        float size() const { return sqrt(pow(x, 2.0) + pow(y, 2.0) + pow(z, 2.0)); }

        //Slow distance function using possible map transfers.
        float distance(const WorldPosition& to) const;

        float fDist(const WorldPosition& to) const;

        //Returns the closest point from the list.
        WorldPosition* closest(const std::vector<WorldPosition*>& list) const { return *std::min_element(list.begin(), list.end(), [this](WorldPosition* i, WorldPosition* j) {return this->distance(*i) < this->distance(*j); }); }
        WorldPosition closest(const std::vector<WorldPosition>& list) const { return *std::min_element(list.begin(), list.end(), [this](WorldPosition i, WorldPosition j) {return this->distance(i) < this->distance(j); }); }

        WorldPosition* furtest(const std::vector<WorldPosition*>& list) const { return *std::max_element(list.begin(), list.end(), [this](WorldPosition* i, WorldPosition* j) {return this->distance(*i) < this->distance(*j); }); }
        WorldPosition furtest(const std::vector<WorldPosition>& list) const { return *std::max_element(list.begin(), list.end(), [this](WorldPosition i, WorldPosition j) {return this->distance(i) < this->distance(j); }); }

        template<class T>
        std::pair<T, WorldPosition>  closest(const std::list<std::pair<T, WorldPosition>>& list) const { return *std::min_element(list.begin(), list.end(), [this](std::pair<T, WorldPosition> i, std::pair<T, WorldPosition> j) {return this->distance(i.second) < this->distance(j.second); }); }
        template<class T>
        std::pair<T, WorldPosition> closest(const std::list<T>& list) const { return closest(GetPosList(list)); }

        template<class T>
        std::pair<T, WorldPosition>  closest(const std::vector<std::pair<T, WorldPosition>>& list) const { return *std::min_element(list.begin(), list.end(), [this](std::pair<T, WorldPosition> i, std::pair<T, WorldPosition> j) {return this->distance(i.second) < this->distance(j.second); }); }
        template<class T>
        std::pair<T, WorldPosition> closest(const std::vector<T>& list) const { return closest(GetPosVector(list)); }

        bool IsWithinDist(const WorldPosition& other, float dist2compare) const { return sqDistance(other) < dist2compare * dist2compare; }

        //Quick square distance in 2d plane.
        float sqDistance2d(const WorldPosition& to) const { return (x - to.x) * (x - to.x) + (y - to.y) * (y - to.y); };

        //Quick square distance calculation without map check. Used for getting the minimum distant points.
        float sqDistance(const WorldPosition& to) const { return (x - to.x) * (x - to.x) + (y - to.y) * (y - to.y) + (z - to.z) * (z - to.z); };

        //Returns the closest point of the list. Fast but only works for the same map.
        WorldPosition* closestSq(const std::vector<WorldPosition*>& list) const { return *std::min_element(list.begin(), list.end(), [this](WorldPosition* i, WorldPosition* j) {return sqDistance(*i) < sqDistance(*j); }); }
        WorldPosition closestSq(const std::vector<WorldPosition>& list) const { return *std::min_element(list.begin(), list.end(), [this](WorldPosition i, WorldPosition j) {return sqDistance(i) < sqDistance(j); }); }

        float getAngleTo(const WorldPosition& endPos) const { float ang = atan2(endPos.y - y, endPos.x - x); return (ang >= 0) ? ang : 2 * M_PI_F + ang; };
        float getAngleBetween(const WorldPosition& dir1, const WorldPosition& dir2) const { return abs(getAngleTo(dir1) - getAngleTo(dir2)); };

        void rotateXY(const float angle) { float nx = cos(angle) * x - sin(angle) * y, ny = sin(angle) * x + cos(angle) * y; x = nx; y = ny; }

        WorldPosition limit(const WorldPosition& center, const float maxDistance) { WorldPosition pos(*this); pos -= center; float size = pos.size(); if (size > maxDistance) { pos /= pos.size(); pos *= maxDistance; pos += center; } return pos; }

        WorldPosition lastInRange(const std::vector<WorldPosition>& list, const float minDist = -1, const float maxDist = -1) const;
        WorldPosition firstOutRange(const std::vector<WorldPosition>& list, const float minDist = -1, const float maxDist = -1) const;

        float mSign(const WorldPosition* p1, const WorldPosition* p2) const { return(x - p2->x) * (p1->y - p2->y) - (p1->x - p2->x) * (y - p2->y); }
        bool isInside(const WorldPosition* p1, const WorldPosition* p2, const WorldPosition* p3) const;

        void distancePartition(const std::vector<float>& distanceLimits, WorldPosition* to, std::vector<std::vector<WorldPosition*>>& partition) const;
        std::vector<std::vector<WorldPosition*>> distancePartition(const std::vector<float>& distanceLimits, std::vector<WorldPosition*> points) const;

        std::vector <WorldPosition*> GetNextPoint(std::vector<WorldPosition*> points, uint32 amount = 1) const;
        std::vector <WorldPosition> GetNextPoint(std::vector<WorldPosition> points, uint32 amount = 1) const;
        
        template<class T>
        void GetNextPoint(std::vector <std::pair<T, WorldPosition*>>& data) const
        {
            std::vector<uint32> weights;

            std::transform(data.begin(), data.end(), std::back_inserter(weights), [this](std::pair<T, WorldPosition*> point) { return 200000 / (1 + this->distance(*point.second)); });

            //If any weight is 0 add 1 to all weights.
            for (auto& w : weights)
            {
                if (w > 0)
                    continue;

                std::for_each(weights.begin(), weights.end(), [](uint32& d) { d += 1; });
                break;
            }

            std::mt19937 gen(time(0));

            WeightedShuffle(data.begin(), data.end(), weights.begin(), weights.end(), gen);
        }

        //Map functions. Player independent.
        const MapEntry* getMapEntry() const { return sMapStorage.LookupEntry<MapEntry>(mapId); }
        uint32 getFirstInstanceId() const { for (auto& map : sMapMgr.Maps()) { if (map.second->GetId() == getMapId()) return map.second->GetInstanceId(); }; return 0; }
        Map* getMap(uint32 instanceId) const { if (!*this) return nullptr; loadMapAndVMap(instanceId); return sMapMgr.FindMap(mapId, instanceId ? instanceId : (getMapEntry()->Instanceable() ? getFirstInstanceId() : 0)); }
        const TerrainInfo* getTerrain() const { return getMap(getFirstInstanceId()) ? getMap(getFirstInstanceId())->GetTerrain() : sTerrainMgr.LoadTerrain(getMapId()); }
        bool isDungeon() { return getMapEntry()->IsDungeon(); }
        float getVisibilityDistance() { return getMap(0) ? getMap(0)->GetVisibilityDistance() : (isOverworld() ? World::GetMaxVisibleDistanceOnContinents() : World::GetMaxVisibleDistanceInInstances()); }

        bool IsInStaticLineOfSight(WorldPosition pos, float heightMod = 0.5f) const;
        bool IsInLineOfSight(WorldPosition pos, float heightMod = 0.5f) const { return mapId == pos.mapId && getMap(getFirstInstanceId()) && getMap(getFirstInstanceId())->isInLineOfSight(x, y, z + heightMod, pos.x, pos.y, pos.z + heightMod, true); }
        bool GetHitPosition(WorldPosition& pos) { return false; /* vmangos Map has no GetHitPosition */ };


        bool isOutside() const { WorldPosition high(*this); high.setZ(z + 500.0f); return IsInLineOfSight(high); }
        bool canFly() const;

float getHeight(bool swim = false) const { return getMap(getFirstInstanceId()) ? getMap(getFirstInstanceId())->GetHeight(x, y, z, !swim) : z; }
float GetHeightInRange(float maxSearchDist = 4.0f) const { return getHeight(); }

float currentHeight() const { return z - getHeight(); }

        std::set<GenericTransport*> getTransports(uint32 entry = 0);
        void CalculatePassengerPosition(GenericTransport* transport);
        void CalculatePassengerOffset(GenericTransport* transport);

        static float GetTransporFloorOffset(uint32 entry);
        void SetTranpotHeightToFloor(uint32 entry) { z += GetTransporFloorOffset(entry); }
        bool isOnTransport(GenericTransport* transport);
        bool SetOnTransport(GenericTransport* transport, int32 startHeight = 10, int32 endHeight = -1);
        WorldPosition RandomPointOnTrans(GenericTransport* transport, float radius, bool findClose, bool useHeight, Player* botForPath, std::vector<WorldPosition>& path);
        WorldPosition RandomPointOnTrans(GenericTransport* transport, float radius = 10.0f, bool findClose = false, bool useHeight = false);

        GridPair getGridPair() const { return MaNGOS::ComputeGridPair(x, y); };
        std::vector<GridPair> getGridPairs(const WorldPosition& secondPos) const;
        static std::vector<WorldPosition> fromGridPair(const GridPair& gridPair, uint32 mapId);

        CellPair getCellPair() const { return MaNGOS::ComputeCellPair(x, y); }
        std::vector<WorldPosition> fromCellPair(const CellPair& cellPair) const;
        std::vector<WorldPosition> gridFromCellPair(const CellPair& cellPair) const;

        mGridPair getmGridPair() const {
            return std::make_pair((int)(32 - x / SIZE_OF_GRIDS), (int)(32 - y / SIZE_OF_GRIDS)); }

        std::vector<mGridPair> getmGridPairs(const WorldPosition& secondPos) const;
        static std::vector<WorldPosition> frommGridPair(const mGridPair& gridPair, uint32 mapId);

        static bool isVmapLoaded(uint32 mapId, int x, int y);

        bool isVmapLoaded() const { return isVmapLoaded(getMapId(), getmGridPair().first, getmGridPair().second); }

        static bool isMmapLoaded(uint32 mapId, uint32 instanceId, int x, int y);

        bool isMmapLoaded(uint32 instanceId) const { return isMmapLoaded(getMapId(), instanceId, getmGridPair().first, getmGridPair().second); }

        static bool loadMapAndVMap(uint32 mapId, uint32 instanceId, int x, int y);
        bool loadMapAndVMap(uint32 instanceId) const {return loadMapAndVMap(getMapId(), instanceId, getmGridPair().first, getmGridPair().second); }
        void loadMapAndVMaps(const WorldPosition& secondPos, uint32 instanceId) const;
        static void unloadMapAndVMaps(uint32 mapId);

        static bool loadVMap(uint32 mapId, int x, int y);
        bool loadVMap() const { return loadVMap(getMapId(), getmGridPair().first, getmGridPair().second); }

        //Display functions
        WorldPosition getDisplayLocation() const;
        float getDisplayX() const { return getDisplayLocation().y * -1.0; }
        float getDisplayY() const { return getDisplayLocation().x; }

        bool isValid() const { return MaNGOS::IsValidMapCoord(x, y, z, o); };
        virtual uint16 getAreaFlag() const {
            loadVMap();
            return isValid() && isVmapLoaded() ? sTerrainMgr.GetAreaFlag(getMapId(), x, y, z) : 0; };
        AreaTableEntry const* GetArea() const;
        std::string getAreaName(const bool fullName = true, const bool zoneName = false) const;
        std::string getAreaOverride() const { return ""; }
        int32 getAreaLevel() const;

        bool HasAreaFlag(const AreaFlags flag = AREA_FLAG_CAPITAL) const;
        bool HasFaction(const Team team) const;

        std::vector<WorldPosition> fromPointsArray(const std::vector<G3D::Vector3>& path) const;
        std::vector<G3D::Vector3> toPointsArray(const std::vector<WorldPosition>& path) const;

        //Pathfinding
        std::vector<WorldPosition> getPathStepFrom(const WorldPosition& startPos, std::unique_ptr<PathFinder>& pathfinder, const Unit* bot, bool forceNormalPath = false) const;
        std::vector<WorldPosition> getPathStepFrom(const WorldPosition& startPos, const Unit* bot, bool forceNormalPath = false) const;
        std::vector<WorldPosition> getPathFromPath(const std::vector<WorldPosition>& startPath, const Unit* bot, const uint8 maxAttempt = 40) const;
        std::vector<WorldPosition> getPathFrom(const WorldPosition& startPos, const Unit* bot) { return getPathFromPath({ startPos }, bot); };
        std::vector<WorldPosition> getPathTo(WorldPosition endPos, const Unit* bot) const { return endPos.getPathFrom(*this, bot); }
        bool isPathTo(const std::vector<WorldPosition>& path, float const maxDistance = 0) const;
        bool cropPathTo(std::vector<WorldPosition>& path, const float maxDistance = 0) const;
        bool canPathTo(const WorldPosition& endPos, const Unit* bot) const { return endPos.isPathTo(getPathTo(endPos, bot)); }

        float getPathLength(const std::vector<WorldPosition>& points) const { float dist = 0.0f; for (auto& p : points) if (&p == &points.front()) dist = 0; else dist += std::prev(&p, 1)->distance(p); return dist; }

        bool ClosestCorrectPoint(float maxRange, float maxHeight = 5.0f, uint32 instanceId = 0);
        bool GetReachableRandomPointOnGround(const Player* bot, const float radius, const bool randomRange = true); //Generic terrain.
        std::vector<WorldPosition> ComputePathToRandomPoint(const Player* bot, const float radius, const bool randomRange = true); //For use with transports.

        uint32 getUnitsAggro(const std::list<ObjectGuid>& units, const Player* bot) const;

        //Creatures
        std::vector<CreatureDataPair const*> getCreaturesNear(const float radius = 0, const uint32 entry = 0) const;
        //GameObjects
        std::vector<GameObjectDataPair const*> getGameObjectsNear(const float radius = 0, const uint32 entry = 0) const;
    };

    inline ByteBuffer& operator<<(ByteBuffer& b, WorldPosition& guidP)
    {
        b << guidP.getMapId();
        b << guidP.x;
        b << guidP.y;
        b << guidP.z;
        b << guidP.o;
        return b;
    }

    inline ByteBuffer& operator>>(ByteBuffer& b, WorldPosition& g)
    {
        uint32 mapId;
        float x;
        float y;
        float z;
        float o;
        b >> mapId;
        b >> x;
        b >> y;
        b >> z;
        b >> o;

        return b;
    }

    //Generic creature finder
    class FindPointCreatureData
    {
    public:
        FindPointCreatureData(WorldPosition point1 = WorldPosition(), float radius1 = 0, uint32 entry1 = 0) { point = point1; radius = radius1; entry = entry1; }

        bool operator()(CreatureDataPair const& dataPair);
        std::vector<CreatureDataPair const*> GetResult() const { return data; };
    private:
        WorldPosition point;
        float radius;
        uint32 entry;

        std::vector<CreatureDataPair const*> data;
    };

    //Generic gameObject finder
    class FindPointGameObjectData
    {
    public:
        FindPointGameObjectData(WorldPosition point1 = WorldPosition(), float radius1 = 0, uint32 entry1 = 0) { point = point1; radius = radius1; entry = entry1; }

        bool operator()(GameObjectDataPair const& dataPair);
        std::vector<GameObjectDataPair const*> GetResult() const { return data; };
    private:
        WorldPosition point;
        float radius;
        uint32 entry;

        std::vector<GameObjectDataPair const*> data;
    };    
}
