#ifndef _PerformanceMonitor_H
#define _PerformanceMonitor_H
#define PMO_MEMTEST

#include "Common.h"
#include "PlayerbotAIBase.h"

#include <atomic>
#include <chrono>
#include <ctime>
#include <iosfwd>
#include <mutex>

namespace ai
{
    enum class BotActivityPhase : uint8
    {
        ActiveUpdate,
        InactiveUpdate,
        ActivityCheck,
        Reaction,
        Decision,
        MinimalDecision,
        MinimalMovement,
        TravelDecision,
        GroundRecovery,
        Count
    };

    class BotActivityTimer
    {
    public:
        static constexpr char LogFile[] = "bot_activity_trace.jsonl";
        static constexpr uint8 SampleInterval = 61;

        explicit BotActivityTimer(BotActivityPhase phase) : phase(phase)
        {
            if (IsEnabled())
                StartSample();
        }

        ~BotActivityTimer()
        {
            if (sampled)
                FinishSample();
        }

        BotActivityTimer(const BotActivityTimer&) = delete;
        BotActivityTimer& operator=(const BotActivityTimer&) = delete;

        static bool IsEnabled() { return enabled.load(std::memory_order_relaxed); }
        static void Enable(bool enable);
        static void WriteSamples(std::ostream& out);

    private:
        void StartSample();
        void FinishSample();

        static std::atomic<bool> enabled;
        std::chrono::steady_clock::time_point start;
        BotActivityPhase phase;
        bool sampled = false;
    };
}

typedef std::vector<std::string> PerformanceStack;

struct PerformanceData
{
    uint32 minTime, maxTime, totalTime, count;
    std::mutex lock;
};

enum PerformanceMetric
{
    PERF_MON_VALUE,
    PERF_MON_TRIGGER,
    PERF_MON_ACTION,
    PERF_MON_RNDBOT,
    PERF_MON_TOTAL
};

class PerformanceMonitorOperation
{
public:
    PerformanceMonitorOperation(PerformanceData& data, std::string name, PerformanceStack* stack);
    ~PerformanceMonitorOperation();

private:
    void finish();

    PerformanceData& data;
    std::string name;
    PerformanceStack* stack;
    std::chrono::milliseconds started;
};

struct VectorStringHash
{
    std::size_t operator()(const std::vector<std::string>& v) const
    {
        std::size_t seed = 0;
        for (const auto& str : v)
        {
            // Combine with XOR and bit shifting (fast)
            seed ^= std::hash<std::string> {}(str) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};

using performanceMap = std::unordered_map<std::vector<std::string>, PerformanceData, VectorStringHash>;
using performanceMetricMap = std::map<PerformanceMetric, performanceMap>;
using performanceInstanceMap = std::map<uint32, performanceMetricMap>;
using performanceMapMap = std::map<uint32, performanceInstanceMap>;

class PerformanceMonitor
{
    public:
        PerformanceMonitor();
        virtual ~PerformanceMonitor();
        static PerformanceMonitor& instance()
        {
            static PerformanceMonitor instance;
            return instance;
        }

    public:
        std::unique_ptr<PerformanceMonitorOperation> start(PerformanceMetric metric, std::string name, PerformanceStack* stack = nullptr, uint32 mapId = 0, uint32 instanceId = 0);
        std::unique_ptr<PerformanceMonitorOperation> start(PerformanceMetric metric, std::string name, PlayerbotAI* ai);
        void PrintStats(bool perTick = false,  bool fullStack = false, bool showMap = false);
        void Reset();
        void Init(uint32 mapId, uint32 instanceId);
    private:
        performanceMetricMap data;
        performanceMapMap mapsData;
        //std::mutex lock;
};


#define sPerformanceMonitor PerformanceMonitor::instance()

#endif

