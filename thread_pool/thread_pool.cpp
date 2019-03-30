// thread_pool.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"

using namespace std;

inline void message(const string& s)
{
	cout << s << endl;
}

template<typename... Args>
string string_format(const Args&... args)
{
	ostringstream os;
	bool dummy[sizeof...(Args)] = { (os << args << ' ', false)... };
	return os.str();
}

template<typename T>
struct Package
{
	T data;
	int id;
	chrono::steady_clock::time_point tp;
	Package(int id, decltype(tp) tp) : data(), id(id), tp(tp) {}
	bool operator< (const Package &rhs) const
	{
		return tp < rhs.tp || (tp == rhs.tp && id < rhs.id);
	}
};
template<typename T>
class Evts
{
	static constexpr chrono::milliseconds TIME_LIMIT = chrono::milliseconds(1000);

	set<Package<T>> st;
	map<int, decltype(st.begin())> id_map;
	int id_cnt = 0;
	std::mutex m;

	void done(int id)
	{
		// lock_guard<mutex> lg(m);

		auto it = id_map.find(id);
		st.erase(it->second);
		id_map.erase(it);

		message(string_format("package", id, "received"));
	}
	void clear_timedouts()
	{
		// lock_guard<mutex> lg(m);

		while (!st.empty())
		{
			auto &ele = *st.begin();
			if (chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - ele.tp) <= TIME_LIMIT) break;

			st.erase(st.begin());
			id_map.erase(ele.id);

			message(string_format("package", ele.id, "timed out"));
		}
	}
public:
	void send_pkg()
	{
		lock_guard<mutex> lg(m);

		auto tp = chrono::steady_clock::now();
		auto ret = st.emplace(++id_cnt, tp);
		assert(ret.second);
		id_map[id_cnt] = ret.first;

		message(string_format("package", id_cnt, "sent"));
	}
	void receive_pkg(int id)
	{
		lock_guard<mutex> lg(m);

		if (id_map.find(id) == id_map.end())
		{
			message(string_format("package", id, "doesn't exists"));
			return;
		}

		done(id);
		clear_timedouts();
	}
};

class ThreadPool
{
	static const int THR_NUM = 10;

	vector<thread> thrs;
	queue<function<void()>> task_q;

	mutex m_for_q;
	condition_variable cv;
	mutex m_for_cv;

	void thr_main()
	{
		unique_lock<mutex> ul(m_for_cv);
		while (true)
		{
			cv.wait(ul, [&]() { return !task_q.empty(); });

			unique_lock<mutex> mtx(m_for_q);
			if (task_q.empty()) 
			{
				message("hit task_q is empty!");
				continue;
			}
			
			auto task = task_q.front();
			task_q.pop();

			mtx.unlock();

			task();
		}
	}
public:
	ThreadPool() : thrs(THR_NUM)
	{
		for (auto &thr : thrs)
		{
			thr = thread(&ThreadPool::thr_main, this);
		}
	}

	void push_task(const function<void()> &task)
	{
		lock_guard<mutex> mtx(m_for_q);
		task_q.emplace(task);

		lock_guard<mutex> mtx2(m_for_cv);
		cv.notify_one();
	}

};

mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());
int gen_rand(int s, int e) { return uniform_int_distribution<int>(s, e)(rng); }

int main()
{
	ThreadPool thr_p;
	Evts<int> timedout_evts;
	int id_cnt = 0;
	while (true)
	{
		auto r_num = gen_rand(1, 5);
		if (r_num == 1)
		{
			thr_p.push_task(bind(&Evts<int>::send_pkg, &timedout_evts));
			++id_cnt;
		}
		else if (r_num == 2)
		{
			thr_p.push_task(bind(&Evts<int>::receive_pkg, &timedout_evts, gen_rand(0, id_cnt)));
		}
		this_thread::sleep_for(chrono::milliseconds(500));
	}
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单
