#include <iostream>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <chrono>
#include <map>
#include <functional>
#include <iomanip>
#include <sstream>
#include <regex>

// Using for convenient current time retrieval
std::string getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm now_tm;
    localtime_s(&now_tm, &now_time_t);
    std::ostringstream oss;
    oss << std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

struct SimpleTask {
    std::string name;
    int delay;
    int priority;
};

struct DelayedTask {
    std::string name;
    int delay;
};

// Comparator for use in the priority queue
struct CompareTasks {
    bool operator()(const std::pair<int, SimpleTask>& lhs, const std::pair<int, SimpleTask>& rhs) const {
        return lhs.first > rhs.first;
    }
};

class PriorityQueue {
private:
    std::priority_queue<std::pair<int, SimpleTask>, std::vector<std::pair<int, SimpleTask>>, CompareTasks> queue;
    std::mutex mtx;
    std::condition_variable cv;
    bool stop = false;

    std::string extractTaskNumber(const std::string& taskName) {
        std::regex re("(\\d+)$");
        std::smatch match;
        if (std::regex_search(taskName, match, re)) {
            return match[1];
        }
        return "";
    }

public:
    void push(const SimpleTask& task) {
        std::lock_guard<std::mutex> lock(mtx);
        queue.push({ task.priority, task });
        cv.notify_one();
    }

    void processTasks() {
        static std::mutex cout_mutex;
        while (true) {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [&] { return !queue.empty() || stop; });

            if (stop && queue.empty()) break;

            auto task = queue.top();
            queue.pop();
            lock.unlock();

            {
                std::lock_guard<std::mutex> cout_lock(cout_mutex);
                std::cout << getCurrentTime() << " : queueS1 : " << task.second.name << " - " << task.second.delay << " running..." << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::seconds(task.second.delay));
            {
                std::lock_guard<std::mutex> cout_lock(cout_mutex);
                std::cout << getCurrentTime() << " : queueS1 : " << task.second.name << " - " << task.second.delay << " completed." << std::endl;
            }

            // Create a delayed task
            createDelayedTask(task.second);
        }
    }

    void stopProcessing() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            stop = true;
        }
        cv.notify_all();
    }

    void createDelayedTask(const SimpleTask& simpleTask) {
        std::string taskNumber = extractTaskNumber(simpleTask.name);
        std::string delayedTaskName = "taskD" + taskNumber;
        {
            static std::mutex cout_mutex;
            std::lock_guard<std::mutex> cout_lock(cout_mutex);
            std::cout << getCurrentTime() << " : " << delayedTaskName << " - 10 : created" << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::seconds(10));
        SimpleTask newSimpleTask = { "taskS" + taskNumber, 2, simpleTask.priority };
        push(newSimpleTask);
        {
            static std::mutex cout_mutex;
            std::lock_guard<std::mutex> cout_lock(cout_mutex);
            std::cout << getCurrentTime() << " : " << delayedTaskName << " - 10 : (" << newSimpleTask.name << " : queueS1) pushed." << std::endl;
        }
    }
};

int main() {
    PriorityQueue queue;

    // Number of worker threads
    const int numWorkers = 4;

    // Launch task processing threads
    std::vector<std::thread> workers;
    for (int i = 0; i < numWorkers; ++i) {
        workers.emplace_back(&PriorityQueue::processTasks, &queue);
    }

    // Add tasks to the queue
    queue.push({ "taskS1", 4, 1 });
    queue.push({ "taskS2", 3, 2 });
    queue.push({ "taskS3", 2, 3 });
    queue.push({ "taskS4", 1, 4 });

    // Stop the threads after all tasks are completed
    for (auto& worker : workers) {
        worker.join();
    }

    return 0;
}
