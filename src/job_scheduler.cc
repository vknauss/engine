#include "job_scheduler.h"

#include <cassert>
#include <cstdlib>

#include <sstream>

#include <chrono>
#include <exception>
#include <functional>
#include <iostream>
#include <random>

//#define VKJOB_DEBUG
#ifdef VKJOB_DEBUG
#define VKJ_DEBUG_PRINT(x) {std::stringstream buf; buf << x << std::endl; std::cout << buf.str();}
#else
#define VKJ_DEBUG_PRINT(x)
#endif // SHEDULER_DEBUG

void JobScheduler::workerThreadMain(JobScheduler* pScheduler, uint32_t id) {
    while (!pScheduler->m_programTerminated) {
        int priority = JOB_PRIORITY_HIGH;
        JobDeclaration decl;

        {
            std::unique_lock<std::mutex> lock(pScheduler->m_threadJobQueues[id].mtx);
            VKJ_DEBUG_PRINT("thread " << id << " checking for work: " << pScheduler->m_threadJobQueues[id].workReady)
            while (!pScheduler->m_threadJobQueues[id].workReady) {

                VKJ_DEBUG_PRINT("thread " << id << " going to sleep")

                pScheduler->m_threadJobQueues[id].cv.wait(lock);

                VKJ_DEBUG_PRINT("thread " << id << " woke up ")
            }

            VKJ_DEBUG_PRINT("thread " << id << " making sure work is legit")

            for (; priority != JOB_PRIORITY_MAX_ENUM; priority++) {
                if (!pScheduler->m_threadJobQueues[id].priorityLevelQueues[priority].empty()) {
                    decl = pScheduler->m_threadJobQueues[id].priorityLevelQueues[priority].front();
                    pScheduler->m_threadJobQueues[id].priorityLevelQueues[priority].pop();
                    break;
                }
            }

            if (priority == JOB_PRIORITY_MAX_ENUM) {
                VKJ_DEBUG_PRINT("thread " << id << " letting us know its unemployed")

                pScheduler->m_threadJobQueues[id].workReady = false;
                pScheduler->m_threadJobQueues[id].cv.notify_all();
            }
        }

        if (priority != JOB_PRIORITY_MAX_ENUM) {
            //std::cout << "decl: " << (uintptr_t)(decl.pFunction) <<  " " << (uintptr_t)(decl.param) <<  " " << decl.numSignalCounters << std::endl;

            VKJ_DEBUG_PRINT("running job (t " << id << ")")
            assert(decl.pFunction != nullptr);

            decl.pFunction(decl.param);

            VKJ_DEBUG_PRINT("done with job (t " << id << ")")

            if (decl.numSignalCounters != 0) {
                for (int i = 0 ; i < decl.numSignalCounters; ++i) {
                    CounterHandle counter = decl.signalCounters[i];
                    if (counter != COUNTER_NULL) {
                        pScheduler->decrementCounter(counter);
                    }
                }
            }

            VKJ_DEBUG_PRINT("thread " << id << " decremented counters")
        } /*else {
            // Save some cycles for the rest of us!
            //std::this_thread::sleep_for(std::chrono::microseconds(10));



            std::lock_guard<std::mutex> lock(pScheduler->m_threadJobQueues[id].mtx);


        }*/
    }
}

void JobScheduler::spawnThreads() {
    auto nThreads = std::thread::hardware_concurrency();

    m_programTerminated = false;
    if (nThreads > 0) {
        m_threadJobQueues.resize(nThreads);

        for (uint32_t i = 0; i < nThreads; ++i) {
            m_workerThreads.push_back(std::thread(workerThreadMain, this, i));
        }
    } else {
        throw std::runtime_error("Unable to detect hardware thread count.");
    }

}

void JobScheduler::joinThreads() {
    std::cout << "Joining worker threads" << std::endl;
    m_programTerminated = true;
    for (uint32_t i = 0; i < m_workerThreads.size(); ++i) {
        {
            std::lock_guard<std::mutex> lock(m_threadJobQueues[i].mtx);
            m_threadJobQueues[i].workReady = true;  // well... not really lol
            m_threadJobQueues[i].cv.notify_all();
        }
        m_workerThreads[i].join();

        VKJ_DEBUG_PRINT("worker thread " << i << " joined")

    }

    std::cout << "Job Scheduler terminated" << std::endl;
}

void JobScheduler::waitForQueues() {
    std::cout << "Waiting for queues" << std::endl;
    for (uint32_t i = 0; i < m_threadJobQueues.size(); ++i) {
        std::unique_lock<std::mutex> lock(m_threadJobQueues[i].mtx);
        while (m_threadJobQueues[i].workReady)
            m_threadJobQueues[i].cv.wait(lock);
        std::cout << "Queue " << i << " finished" << std::endl;
    }
    std::cout << "All queues finished" << std::endl;
}

uint32_t JobScheduler::getRandomThread() const {
    static thread_local std::mt19937 generator((uint_fast32_t) (std::hash<std::thread::id>{}(std::this_thread::get_id())));
    std::uniform_int_distribution<uint32_t> dist(0, m_workerThreads.size()-1);
    return dist(generator);
}

void JobScheduler::enqueueJob(JobScheduler::JobDeclaration decl) {
    bool wait = false;
    if (decl.waitCounter != COUNTER_NULL) {
        std::shared_lock<std::shared_mutex> slock(m_allCountersMtx);
        std::lock_guard<std::mutex> lock(m_counters[decl.waitCounter].mtx);
        std::lock_guard<std::shared_mutex> wlock(m_waitListsMtx);
        if (m_counters[decl.waitCounter].count > 0) {
            m_counterWaitLists[decl.waitCounter].push_front(decl);
            wait = true;
        }
    }

    if (decl.numSignalCounters > 0) {
        std::shared_lock<std::shared_mutex> slock(m_allCountersMtx);
        for (int i = 0; i < decl.numSignalCounters; ++i) {
            CounterHandle counter = decl.signalCounters[i];
            if (counter != COUNTER_NULL) {
                std::lock_guard<std::mutex> lock(m_counters[counter].mtx);
                ++m_counters[counter].count;

                VKJ_DEBUG_PRINT("Counter " << (m_counters[counter].hasID ? m_counters[counter].id : std::to_string(counter)) << " incremented. Value: " << m_counters[counter].count)

            }
        }
    }

    if (wait) return;

    //if (decl.pFunction == nullptr) std::cout << "Howdy!" << msg << std::endl;

    uint32_t thread = getRandomThread();

    std::lock_guard<std::mutex> lock(m_threadJobQueues[thread].mtx);

    VKJ_DEBUG_PRINT("adding job to thread " << thread << " q " << decl.priority)

    m_threadJobQueues[thread].priorityLevelQueues[decl.priority].push(decl);
    m_threadJobQueues[thread].workReady = true;
    m_threadJobQueues[thread].cv.notify_all();

    VKJ_DEBUG_PRINT("Thread " << thread << " notified")

}

void JobScheduler::enqueueJobs(uint32_t count, JobScheduler::JobDeclaration* pDecls, bool ignoreSignals) {
    uint32_t startThread = getRandomThread();



    // Determine which jobs need to wait
    // These jobs will be added to the central wait lists and not enqueued
    std::map<CounterHandle, std::forward_list<uint32_t>> waitLists;

    std::vector<std::forward_list<CounterHandle>> waitCounters(count);

    for (uint32_t i = 0; i < count; ++i) {
        if (pDecls[i].waitCounter != COUNTER_NULL) {
            waitLists[pDecls[i].waitCounter].push_front(i);
        }
    }

    std::vector<bool> isWaiting(count, false);
    uint32_t waitCount = 0;

    {
        std::shared_lock<std::shared_mutex> slock(m_allCountersMtx);
        for (auto kv : waitLists) {
            std::lock_guard<std::mutex> lock(m_counters[kv.first].mtx);
            std::lock_guard<std::shared_mutex> wlock(m_waitListsMtx);
            if (m_counters[kv.first].count > 0) {
                for (auto ind : kv.second) {
                    isWaiting[ind] = true;
                    m_counterWaitLists[kv.first].push_front(pDecls[ind]);
                    ++waitCount;
                }
            }
        }
    }


    // Signal all the counters for jobs that are being enqueued
    // Sort the declarations by counter, since in some cases many may all share the same counter(s)
    if (!ignoreSignals) {
        std::map<CounterHandle, uint32_t> signalCounts;

        for (uint32_t i = 0; i < count; ++i) {
            if (/* !isWaiting[i] && */pDecls[i].numSignalCounters > 0) {
                for (int j = 0; j < pDecls[i].numSignalCounters; ++j) {
                    CounterHandle counter = pDecls[i].signalCounters[j];
                    ++signalCounts[counter];
                }
            }
        }

        {
            std::shared_lock<std::shared_mutex> slock(m_allCountersMtx);
            for (auto kv : signalCounts) {
                std::lock_guard<std::mutex> lock(m_counters[kv.first].mtx);
                m_counters[kv.first].count += kv.second;

                VKJ_DEBUG_PRINT("Counter " << kv.first << " incremented. Value: " << m_counters[kv.first].count)

            }
        }
    }

    // Enqueue all non-waiting jobs by splitting between all worker threads
    uint32_t ind = 0;
    uint32_t fnum = (count - waitCount >= m_workerThreads.size()) ? (uint32_t) (count - waitCount) / m_workerThreads.size() : 0;
    uint32_t remainder = (count - waitCount) - (m_workerThreads.size() * fnum);
    for (uint32_t i = 0; i < m_workerThreads.size(); ++i) {
        uint32_t thread = (startThread + i) % m_workerThreads.size();

        uint32_t num = fnum;
        if (i < remainder) num += 1;

        //if ((i+1) * num > count - waitCount) num = (count - waitCount) - i * num;
        if (num == 0) break;

        VKJ_DEBUG_PRINT("Thread " << thread << " gets " << num << " jobs")

        std::lock_guard<std::mutex> lock(m_threadJobQueues[thread].mtx);
        for (uint32_t j = ind, cct = 0; j < count && cct < num && ind + cct < count - waitCount; ++j) {
            if (isWaiting[j]) continue;
            m_threadJobQueues[thread].priorityLevelQueues[pDecls[j].priority].push(pDecls[j]);
            ++cct;
        }

        m_threadJobQueues[thread].workReady = true;
        m_threadJobQueues[thread].cv.notify_all();

        VKJ_DEBUG_PRINT("Thread " << thread << " notified")

        ind += num;
        if (ind >= count) break;
    }

    VKJ_DEBUG_PRINT("Assigned " << ind << " out of " << count << " jobs. Good nuff? " << waitCount << " are supposed to wait")

}

JobScheduler::CounterHandle JobScheduler::getCounterByID(std::string id) {
    uint32_t idHash = std::hash<std::string>{}(id);
    std::lock_guard<std::mutex> hashLock(m_countersMapMtx);
    if (auto it = m_countersByHashID.find(idHash); it != m_countersByHashID.end()) {
        return it->second;
    } else {
        CounterHandle handle = getFreeCounter();
        std::shared_lock<std::shared_mutex> slock(m_allCountersMtx);
        std::lock_guard<std::mutex> lock(m_counters[handle].mtx);
        m_counters[handle].hasID = true;
        m_countersByHashID[idHash] = handle;
        m_counters[handle].id = id;

        //std::cout << id << " " << handle << std::endl;

        return handle;
    }
}

JobScheduler::CounterHandle JobScheduler::getFreeCounter() {
    {
        std::lock_guard<std::mutex> lock(m_freeCounterMtx);
        if (!m_freeCounters.empty()) {
            CounterHandle handle = m_freeCounters.front();
            m_freeCounters.pop_front();
            return handle;
        }
    }

    std::lock_guard<std::shared_mutex> lock(m_allCountersMtx);
    CounterHandle handle = m_counters.size();
    m_counters.emplace_back();
    m_counters.back().handle = handle;
    return handle;
}

void JobScheduler::decrementCounter(JobScheduler::CounterHandle handle) {
    std::vector<JobDeclaration> waitJobs;
    {
        std::shared_lock<std::shared_mutex> slock(m_allCountersMtx);
        std::lock_guard<std::mutex> lock(m_counters[handle].mtx);
        if ((--m_counters[handle].count) == 0) {
            std::shared_lock<std::shared_mutex> wlock(m_waitListsMtx);
            if (auto kv = m_counterWaitLists.find(handle); kv != m_counterWaitLists.end()) {
                waitJobs.assign(kv->second.begin(), kv->second.end());
                kv->second.clear();
            }

            VKJ_DEBUG_PRINT("Counter " << (m_counters[handle].hasID ? m_counters[handle].id : std::to_string((int)handle)) << " hit zero. Enqueuing " << waitJobs.size() << " jobs.")
        } else {
            VKJ_DEBUG_PRINT("Counter " << (m_counters[handle].hasID ? m_counters[handle].id : std::to_string((int)handle)) << " decremented. Value: " << m_counters[handle].count)
        }
    }
    if (!waitJobs.empty()) enqueueJobs(waitJobs.size(), waitJobs.data(), true);
}

void JobScheduler::freeCounter(JobScheduler::CounterHandle handle) {
    std::shared_lock<std::shared_mutex> slock(m_allCountersMtx);
    if (handle < m_counters.size()) {
        std::lock_guard<std::mutex> lock(m_counters[handle].mtx);
        std::lock_guard<std::mutex> freeListLock(m_freeCounterMtx);
        m_freeCounters.push_front(handle);
        m_counters[handle].count = 0;
        m_counters[handle].hasID = false;
    }
}
