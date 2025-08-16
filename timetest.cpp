
#define WAIT_MS (100)
#define N_PASS  (20)

#define COL_1	(25)
#define COL_N	(12)

#include <iostream>
#include <string>
#include <windows.h>
#include <winnt.h>
#include <unistd.h>

using std::string; using std::cout; using std::endl; using std::to_string;

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

void test_wtimer()
{
	// Same result with 0, 1 or 16, timeBeginPeriod seems totally ignored, and no performance impact calling it
	unsigned int per = 0;
	if (per) timeBeginPeriod(per); 
	//TODO: handle error (waitable high resolution timer supported only from W10 1803)
	HANDLE hTimer = CreateWaitableTimerEx(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
	LARGE_INTEGER dueTime; 
	dueTime.QuadPart = -10000 * WAIT_MS; // "in 100 nanoseconds interval", negative = relative value
	SetWaitableTimer(hTimer, &dueTime, 0 /*once*/, NULL, NULL, FALSE);	
	WaitForSingleObject(hTimer, INFINITE);
	CloseHandle(hTimer);
	if (per) timeEndPeriod(per);
}

void test_nanosleep()
{
	timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = 1000000 * WAIT_MS;

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
	if (usleep(1000 * WAIT_MS)==-1)
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
	for (int pass = 0; pass < N_PASS ;++pass)
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
		<< rpad(to_string(avg),COL_N," ") << rpad(to_string(max),COL_N," ") << rpad(to_string(max - min),COL_N," ") << endl;	
}

int main(int argc, char** argv)
{
	QueryPerformanceFrequency(&QPfreq);
	cout << "QPC Timer is " << QPfreq.QuadPart << " Hz" << endl;
	//TODO: show timeBeginPeriod current setting / best setting (timeGetDevCaps)
	cout << lpad("QPC ticks",COL_1," ") << rpad("MIN",COL_N," ") << rpad("AVG",COL_N," ") << rpad("MAX",COL_N," ") << rpad("MAX VAR",COL_N," ") << endl;
	//cout << "Testing empty routine..." << endl;
	//bench_wait("Nothing",nothing);
	cout << "Testing " << WAIT_MS << "ms wait routines, " << N_PASS << " pass..." << endl;
	bench_wait("WIN32 Sleep()",test_sleep);
	bench_wait("High-res waitable timer",test_wtimer);
	bench_wait("MinGW usleep()",test_usleep);
	bench_wait("MinGW nanosleep()",test_nanosleep);
	bench_wait("QPC busy wait",test_busywait);
	return 0;
}

