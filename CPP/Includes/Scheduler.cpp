#include "Std.cpp"

class Scheduler {
public:
    using Task = function<void(usize)>;
    using Clock = chrono::steady_clock;
    using TimePoint = Clock::time_point;

    Scheduler() {
        running = true;
        worker = thread(&Scheduler::run, this);
    }

    ~Scheduler() {
        {
            lock_guard lock(mutex);

			running = false;
            condition.notify_all();
        }

        if(worker.joinable()) {
            worker.join();
        }
    }

    usize schedule(Task task, int delayMS = 0, bool repeat = false, bool immediate = false) {
        auto ST = SP<ScheduledTask>();

        {
            lock_guard lock(mutex);

            ST->ID = nextID++;

			if(task) {
				ST->task = task;
			} else {
				println("[Scheduler] Adding empty task /!\\");
			}

            ST->delayMS = delayMS;
            ST->repeat = repeat;
            ST->time = Clock::now()+chrono::milliseconds(immediate ? 0 : delayMS);

            tasks[ST->ID] = ST;
            queue.push(ST);
            condition.notify_one();
        }

        return ST->ID;
    }

    void await(usize ID) {
        sp<ScheduledTask> ST;

		{
            lock_guard lock(mutex);
            auto it = tasks.find(ID);

            if(it == tasks.end()) {
				return;
			}

            ST = it->second;
        }

        unique_lock lock(mutex);

		if(!ST->cancel) {
			ST->condition.wait(lock);
		}
    }

    void cancel(usize ID) {
        lock_guard lock(mutex);
        auto it = tasks.find(ID);

        if(it != tasks.end()) {
            auto ST = it->second;

			ST->cancel = true;
            ST->condition.notify_all();

			tasks.erase(it);
        }
    }

private:
    struct ScheduledTask {
        usize ID;
        Task task;
        TimePoint time;
        int delayMS = 0;
        bool repeat = false,
			 cancel = false;
        condition_variable condition;
    };

    struct Compare {
        bool operator()(sp<ScheduledTask> LHS, sp<ScheduledTask> RHS) const {
            return LHS->time > RHS->time;
        }
    };

    std::mutex mutex;
    unordered_map<usize, sp<ScheduledTask>> tasks;
    priority_queue<sp<ScheduledTask>, vector<sp<ScheduledTask>>, Compare> queue;
    condition_variable condition;
    atomic<bool> running;
    thread worker;
    int nextID = 0;

    void run() {
        while(running) {
            sp<ScheduledTask> ST;
			unique_lock lock(mutex);

			while(queue.empty() && running) {
				condition.wait(lock);
			}

			if(!running) {
				break;
			}

			ST = queue.top();

			if(ST->time > Clock::now()) {
				condition.wait_until(lock, ST->time);

				continue;
			}

			queue.pop();

			if(!ST->cancel && ST->task) {
				try {
					ST->task(ST->ID);
				} catch(const exception& e) {
					println("[Scheduler] Exception in task ", ST->ID, ": ", e.what());
				}
			}

			ST->condition.notify_all();

			if(!ST->cancel && ST->repeat) {
				ST->time = Clock::now()+chrono::milliseconds(ST->delayMS);

				queue.push(ST);
				condition.notify_one();
			} else {
				tasks.erase(ST->ID);
			}
        }
    }
};

static Scheduler sharedScheduler;