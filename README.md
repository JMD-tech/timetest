### Description

A quick test to compare different ways to implement a "nanosecond" wait **resolution** 
(obviously not this accuracy, but the closer the better) in a Windows C/C++ program, both with native WIN32 API and MinGW.  

TL;DR  
High-resolution waitable timers are the most consistent and accurate (in all situations), yet it's much unrealistic to expect sub-millisecond resolution under all circumstances, specially under heavy load.

### Context

While working on porting the excellent [Roc Toolkit](https://roc-streaming.org/) for Windows,
testing different methods to implement accurate short wait routines.

Tests done on a laptop with i5-1135G7 CPU, Windows 11 24H2  
```
$ gcc --version  
gcc.exe (Rev1, Built by MSYS2 project) 15.1.0  
```
Some other tests on an ASUS ROG with i7-7700 and Windows 10 22H2

### Usage

```timetest.exe```

You can specify the wait time in ms and number of passes as command-line arguments:

```timetest.exe 10 500```

For 500 waits of 10 ms.
Default values are 50 waits of 100 ms.

Example result:

```
$ ./timetest.exe
QPC Timer is 10000000 Hz, 100 ns tick
### TEST OF 50 x 100 ms wait routine runs
QPC ticks                         MIN         AVG         MAX    MAX OVER
WIN32 Sleep()                 1000568     1007637     1017800       17800
Norm-res waitable timer        999072     1004889     1013827       13827
High-res waitable timer       1001433     1004804     1007992        7992
MinGW usleep()                1000981     1007273     1014476       14476
MinGW nanosleep()             1060380     1094681     1161560      161560
QPC busy wait                 1000004     1000008     1000015          15
HR wait timer + busywait      1000003     1000007     1000015          15
```

note: Will probably give many errors on Windows versions older than Windows 10 1803, as they don't support high-resolution timers.


### Known shortcomings of the "testing methodology"

- QPC ticks are used as the absolute measure of elapsed time, so it will obviously favor routines based on the same clock source versus other ones.  

- As seen by some MAX VAR results on the QPC busy_wait routine, 500 runs is not enough for catching some worst-case events on this one.
Should really run much more extensive tests over some hours with a distribution graph of actual elapsed time over just an AVG / MAX metric.  

- Test limited to 1 ms test routines, i'll have to test the same/equivelent on native Linux if it does better, but anyway all of those routines on Windows, even those claiming higher resolution have actual accuracy about useless for sub-millisecond, at least in standard desktop OS configuration.  

- MAX VAR isn't really a good metric, if we intended "wait at least xx nanoseconds" it should be MAX OVERSHOOT, here it's more of a coherency/repeatability test of the same routine. update: done in latest version.  

- Most people having studied the subject will notice no tests or mention of **timeBeginPeriod()** topic.  
Tested it with different values and **noticed no effect**, i suppose either my most recent W11 set it by default to 1, 
or i've already some software/service started at boot/session login that does set it to 1.  
Of some few tests that i've done, the **only slight difference** i've found on my machine is **when battery-powered** vs mains power plugged, 
but not that much (didn't run extensive enough tests to have significant statistical evidence - no need as some niche (sampling with total galvanic isolation?) 
application requiring both this time accuracy and battery operation would either set timeBeginPeriod itself or have admins tweaking some registry settings).  

### Notable results

You can see the full results in the tests subdirectory.  

- No comment on the busy wait routine, **can be** very accurate but eek! (And its accuracy **totally crumbles under heavy CPU load**), to the point i gave up the idea of a two-phase wait (slight undershoot with HR waitable timer and completing with busy-wait and use HR timer solo).  

- Even imagining **nanosleep()** using some other clock source, doesn't explain such **huge discrepancy** (10 ms waits averaging at 15 ms, with maximum of 30 ms), specially on small waits (1 - 10 ms).  

- WIN32 Sleep() and MinGW usleep() are really close, at least at a common resolution.  

- WIN32 Sleep() is slightly more accurate than non high-res timer based routines, but those with **high-res** (CREATE_WAITABLE_TIMER_HIGH_RESOLUTION) are **really better, yet very far from nominal 100 ns resolution**.  

- Both WIN32 Sleep, waitable timers and usleep cope really well with ALL-threads CPU load of lower priority. The scheduler does a good job :)
All methods except nanosleep and busy wait even got slightly more accurate, might be those lower-priority loads preventing the threads/cores from entering some sleep modes?  

- With threads of normal (=same) priority, all lose consistency and accuracy, with some 10ms waits becoming in the 30ms. That is to be expected in a non real-time OS and still acceptable for most applications. Critical applications should be started with a higher than default priority.  

- Tested mostly out of curiosity, as it's never considered a "normal mode of operation" but rather occurs "in the wild" from software/OS malfunction or DoS, but did test with all CPU-threads 100% loaded by higher priority processes.  
To my suprise, all except nanosleep and busy wait somehow fared better in this situation than in competition with normal/same priority loads.  
