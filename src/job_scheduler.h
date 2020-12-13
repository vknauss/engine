#ifndef JOB_SCHEDULER_H_
#define JOB_SCHEDULER_H_

#include <array>
#include <atomic>
#include <condition_variable>
#include <forward_list>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

class JobScheduler {

public:

    enum PriorityLevel {
        JOB_PRIORITY_HIGH,
        JOB_PRIORITY_NORMAL,
        JOB_PRIORITY_LOW,
        JOB_PRIORITY_MAX_ENUM
    };

    typedef void JobFunction (uintptr_t);

    typedef uint32_t CounterHandle;
    static constexpr CounterHandle COUNTER_NULL = std::numeric_limits<uint32_t>::max();

    static constexpr int MAX_COUNTERS = 4;

    struct JobDeclaration {
        JobFunction* pFunction = nullptr;
        uintptr_t param = 0;
        PriorityLevel priority = JOB_PRIORITY_NORMAL;
        CounterHandle waitCounter = COUNTER_NULL;
        CounterHandle signalCounters[MAX_COUNTERS] = {COUNTER_NULL};
        int numSignalCounters = 0;
    };

    JobScheduler() :
        m_workerThreads(),
        m_threadJobQueues(),
        m_counters(),
        m_freeCounters(),
        m_countersByHashID(),
        m_allCountersMtx(),
        m_freeCounterMtx(),
        m_countersMapMtx(),
        m_waitListsMtx(),
        m_counterWaitLists(),
        m_programTerminated(false)
    {

    }

    void spawnThreads();

    void joinThreads();

    void waitForQueues();

    void enqueueJob(JobDeclaration decl);

    void enqueueJobs(uint32_t count, JobDeclaration* pDecls, bool ignoreSignals = false);

    CounterHandle getCounterByID(std::string id);

    CounterHandle getFreeCounter();

    void freeCounter(CounterHandle handle);

private:

    struct JobQueue {
        std::array<std::queue<JobDeclaration>, JOB_PRIORITY_MAX_ENUM> priorityLevelQueues;
        std::mutex mtx;
        std::condition_variable cv;
        bool workReady;

        JobQueue() : priorityLevelQueues(), mtx(), cv(), workReady(false) {}

        JobQueue(JobQueue&& j) {
            std::lock_guard<std::mutex> olock(j.mtx);
            std::lock_guard<std::mutex> mlock(mtx);
            priorityLevelQueues = std::move(j.priorityLevelQueues);
            workReady = std::move(j.workReady);
        }
    };

    struct Counter {
        std::mutex mtx;
        CounterHandle handle;
        std::string id;
        uint32_t count;
        bool hasID;

        Counter() : mtx(), handle(0), id(), count(0), hasID(false) {}

        Counter(Counter&& c) {
            std::lock_guard<std::mutex> olock(c.mtx);
            std::lock_guard<std::mutex> mlock(mtx);
            handle = std::move(c.handle);
            count = std::move(c.count);
            hasID = std::move(c.hasID);
            id = std::move(c.id);
        }
    };

    std::vector<std::thread> m_workerThreads;
    std::vector<JobQueue> m_threadJobQueues;

    std::vector<Counter> m_counters;
    std::forward_list<CounterHandle> m_freeCounters;
    std::map<size_t, CounterHandle> m_countersByHashID;

    std::shared_mutex m_allCountersMtx;
    std::mutex m_freeCounterMtx;
    std::mutex m_countersMapMtx;
    std::shared_mutex m_waitListsMtx;

    std::map<CounterHandle, std::forward_list<JobDeclaration>> m_counterWaitLists;

    std::atomic_bool m_programTerminated;

    void decrementCounter(CounterHandle handle);

    uint32_t getRandomThread() const;

    static void workerThreadMain(JobScheduler* pScheduler, uint32_t id);

};

#endif // JOB_SCHEDULER_H_
