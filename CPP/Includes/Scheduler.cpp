#pragma once

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

	usize schedule(Task task, int delayMS = 0, bool cycled = false, bool immediate = false) {
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
			ST->cycled = cycled;
			ST->time = Clock::now()+chrono::milliseconds(immediate ? 0 : delayMS);

			tasks[ST->ID] = ST;
			queue.insert(ST);
			condition.notify_one();
		}

		return ST->ID;
	}

	void reset(usize ID) {
		lock_guard lock(mutex);
		auto it = tasks.find(ID);

		if(it != tasks.end()) {
			auto ST = it->second;

			if(ST->cancelled) {
				return;
			}

			queue.erase(ST);

			ST->time = Clock::now()+chrono::milliseconds(ST->delayMS);

			queue.insert(ST);
			condition.notify_one();
		}
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

		while(!ST->cancelled && !ST->finished) {
			ST->condition.wait(lock);
		}
	}

	void cancel(usize ID) {
		lock_guard lock(mutex);
		auto it = tasks.find(ID);

		if(it != tasks.end()) {
			auto ST = it->second;

			ST->cancelled = true;
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
		bool cycled = false,
			 cancelled = false,
			 finished = false;
		condition_variable condition;
	};

	struct Compare {
		bool operator()(const sp<ScheduledTask> LHS, const sp<ScheduledTask> RHS) const {
			if(LHS->time == RHS->time) {
				return LHS->ID < RHS->ID;
			}

			return LHS->time < RHS->time;
		}
	};

	std::mutex mutex;
	unordered_map<usize, sp<ScheduledTask>> tasks;
	set<sp<ScheduledTask>, Compare> queue;
	condition_variable condition;
	atomic<bool> running;
	thread worker;
	int nextID = 0;

	void run() {
		prctl(PR_SET_NAME, "Scheduler", 0, 0, 0);

		while(running) {
			sp<ScheduledTask> ST;
			unique_lock lock(mutex);

			while(queue.empty() && running) {
				condition.wait(lock);
			}

			if(!running) {
				break;
			}

			auto it = queue.begin();
			ST = *it;

			if(ST->time > Clock::now()) {
				condition.wait_until(lock, ST->time);

				continue;
			}

			queue.erase(it);

			if(!ST->cancelled && ST->task) {
				try {
					lock.unlock(); // Allow tasks to call schedule/await/cancel
					ST->task(ST->ID);
					lock.lock();
				} catch(const exception& e) {
					println("[Scheduler] Exception in task ", ST->ID, ": ", e.what());
				}
			}

			ST->finished = true;
			ST->condition.notify_all();

			if(!ST->cancelled && ST->cycled) {
				ST->time = Clock::now()+chrono::milliseconds(ST->delayMS);
				ST->finished = false;

				queue.insert(ST);
				condition.notify_one();
			} else {
				tasks.erase(ST->ID);
			}
		}
	}
};

static Scheduler sharedScheduler;