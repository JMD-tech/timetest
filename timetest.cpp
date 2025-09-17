
#define COL_1	(25)
#define COL_N	(12)

#include <iostream>
#include <string>
#include <vector>
#include <windows.h>
#include <winnt.h>
#include <stdlib.h>
#include <unistd.h>

unsigned int WAIT_MS = 100, N_PASS = 50;

using std::string; using std::cout; using std::endl; using std::to_string; using std::vector;

string lpad( const string& s, unsigned int n, const string& c )
{
	string resu=s;
	while (resu.size()<n) resu+=c;
	return resu.substr(0,n);
}

string rpad( const string& s, unsigned int n, const string& c )
{
	string resu=s;
	while (resu.size()<n) resu=c+resu;
	return resu.substr(resu.size()-n,n);
}

LARGE_INTEGER QPfreq;	// Global as we'll use it in busy wait too

LONGLONG ticks()
{
	LARGE_INTEGER tmp;
	if (!QueryPerformanceCounter(&tmp))
		return -1;
	return tmp.QuadPart;
}

void nothing() {}

void test_sleep() { Sleep(WAIT_MS); }

void test_wtimer_lr()
{
	HANDLE hTimer = CreateWaitableTimerEx(NULL, NULL, 0, TIMER_ALL_ACCESS);
	LARGE_INTEGER dueTime;
	dueTime.QuadPart = -10000 * (LONGLONG)WAIT_MS; // "in 100 nanoseconds interval", negative = relative value
	SetWaitableTimer(hTimer, &dueTime, 0 /*once*/, NULL, NULL, FALSE);
	WaitForSingleObject(hTimer, INFINITE);
	CloseHandle(hTimer);
}

void test_wtimer_hr()
{
	// Same result with 0, 1 or 16, timeBeginPeriod seems totally ignored, and no performance impact calling it
	unsigned int per = 0;
	if (per) timeBeginPeriod(per);
	//TODO: handle error (waitable high resolution timer supported only from W10 1803)
	HANDLE hTimer = CreateWaitableTimerEx(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
	LARGE_INTEGER dueTime;
	dueTime.QuadPart = -10000 * (LONGLONG)WAIT_MS; // "in 100 nanoseconds interval", negative = relative value
	SetWaitableTimer(hTimer, &dueTime, 0 /*once*/, NULL, NULL, FALSE);
	WaitForSingleObject(hTimer, INFINITE);
	CloseHandle(hTimer);
	if (per) timeEndPeriod(per);
}

#define UNDERSHOOT_US (1000)

void test_wtimer_hr_plus_busywait()
{
	LONGLONG etime = ticks() + (((LONGLONG)WAIT_MS * QPfreq.QuadPart) / 1000L);

	if ((LONGLONG)WAIT_MS * 1000L > (LONGLONG)UNDERSHOOT_US)
	{
		// Do a timer wait with slight undershoot
		HANDLE hTimer = CreateWaitableTimerEx(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
		LARGE_INTEGER dueTime;
		dueTime.QuadPart = -10000 * (LONGLONG)WAIT_MS + 10 * (LONGLONG)UNDERSHOOT_US; // "in 100 nanoseconds interval", negative = relative value
		SetWaitableTimer(hTimer, &dueTime, 0 /*once*/, NULL, NULL, FALSE);
		WaitForSingleObject(hTimer, INFINITE);
		CloseHandle(hTimer);
	}

	// Then finish waiting in busy wait
	while (ticks()<etime) {}
}

void test_nanosleep()
{
	timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = 1000000L * (LONGLONG)WAIT_MS;

	//if (nanosleep(&ts, &ts)==-1)
	//	cout << "nanosleep failed" << endl;
	// No significant perf impact of the comparison and loop

	while (nanosleep(&ts, &ts) == -1)
		if (errno != EINTR)
		{
			cout << "nanosleep failed" << endl;
			return;
		}
}

void test_usleep()
{
	// Should check with some signals disabled, or exclude failed samples from stat data, but didn't happen so...
	if (usleep(1000L * (LONGLONG)WAIT_MS)==-1)
		cout << "usleep() failed." << endl;
}

void test_busywait()
{
	LONGLONG waitfor = ((WAIT_MS * QPfreq.QuadPart) / 1000) + ticks();
	while (ticks()<waitfor) {}
}

void bench_wait(const char *nam, void (*func)())
{
	LONGLONG min=0,max=0,tot=0,avg;
	for (unsigned int pass = 0; pass < N_PASS ;++pass)
	{
		LONGLONG tbefore = ticks();
		func();
		LONGLONG tafter = ticks(), elapsed = tafter - tbefore;
		if (!min || (elapsed<min)) min = elapsed;
		if (elapsed>max) max=elapsed;
		tot+=elapsed;
	}
	avg = tot / N_PASS;
	cout << lpad(nam,COL_1," ") << rpad(to_string(min),COL_N," ")
		<< rpad(to_string(avg),COL_N," ") << rpad(to_string(max),COL_N," ") 
		<< rpad(to_string(max - ((LONGLONG)WAIT_MS * 10000L)),COL_N," ") << endl;
}

int main(int argc, char** argv)
{
	vector<string> args(argv + 1, argv + argc);
	if (args.size())
	{
		int tmp = atoi(args[0].c_str());
		if (!tmp)
		{
			cout << args[0] << " : Invalid value for WAIT_MS" << endl;
			return 1;
		}
		WAIT_MS = tmp;
		if (args.size()>1)
		{
			tmp = atoi(args[1].c_str());
			if (!tmp)
			{
				cout << args[1] << " : Invalid value for N_PASS" << endl;
				return 1;
			}
			N_PASS = tmp;
		}
	}
	
	QueryPerformanceFrequency(&QPfreq);
	cout << "QPC Timer is " << QPfreq.QuadPart << " Hz, " << 1000000000L / QPfreq.QuadPart << " ns tick" << endl;
	//TODO: show timeBeginPeriod current setting / best setting (timeGetDevCaps)
	cout << "### TEST OF " << N_PASS << " x " << WAIT_MS << " ms wait routine runs" << endl;
	cout << lpad("QPC ticks",COL_1," ") << rpad("MIN",COL_N," ") << rpad("AVG",COL_N," ") << rpad("MAX",COL_N," ") << rpad("MAX OVER",COL_N," ") << endl;
	//cout << "Testing empty routine..." << endl;
	//bench_wait("Nothing",nothing);
	bench_wait("WIN32 Sleep()",test_sleep);
	bench_wait("Norm-res waitable timer",test_wtimer_lr);
	bench_wait("High-res waitable timer",test_wtimer_hr);
	bench_wait("MinGW usleep()",test_usleep);
	bench_wait("MinGW nanosleep()",test_nanosleep);
	bench_wait("QPC busy wait",test_busywait);
	bench_wait("HR wait timer + busywait",test_wtimer_hr_plus_busywait);
	return 0;
}

