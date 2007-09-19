#include <windows.h>
#include <assert.h>

#include "include/asm/callbacks.h"

struct _thread_info {
        HANDLE th;
        HANDLE sched_sem;
	int dead;
};

struct kernel_thread_helper_arg {
        int (*fn)(void*);
        void *arg;
        struct _thread_info *pti;
};


void* linux_thread_info_alloc(void)
{
        struct _thread_info *pti=malloc(sizeof(*pti));

	assert(pti != NULL);

        pti->sched_sem=CreateSemaphore(NULL, 0, 100, NULL);
	pti->dead=0;

	return pti;
}

int debug_thread_count;

void linux_context_switch(void *prev, void *next)
{
        struct _thread_info *_prev=(struct _thread_info*)prev;
        struct _thread_info *_next=(struct _thread_info*)next;

        ReleaseSemaphore(_next->sched_sem, 1, NULL);
        WaitForSingleObject(_prev->sched_sem, INFINITE);
	if (_prev->dead) {
		CloseHandle(_prev->sched_sem);
		free(_prev);
		debug_thread_count--;
		ExitThread(0);
	}

}

HANDLE kth_sem;

DWORD WINAPI kernel_thread_helper(LPVOID arg)
{
        struct kernel_thread_helper_arg *ktha=(struct kernel_thread_helper_arg*)arg;
        int (*fn)(void*)=ktha->fn;
        void *farg=ktha->arg;
        struct _thread_info *pti=ktha->pti;

        ReleaseSemaphore(kth_sem, 1, NULL);
        WaitForSingleObject(pti->sched_sem, INFINITE);
        return fn(farg);
}

void linux_free_thread(void *arg)
{
        struct _thread_info *pti=(struct _thread_info*)arg;

	pti->dead=1;
	ReleaseSemaphore(pti->sched_sem, 1, NULL);
}

int linux_new_thread(int (*fn)(void*), void *arg, void *pti)
{
        struct kernel_thread_helper_arg ktha = {
                .fn = fn,
                .arg = arg,
                .pti = (struct _thread_info*)pti
        };


	debug_thread_count++;

        ktha.pti->th=CreateThread(NULL, 0, kernel_thread_helper, &ktha, 0, NULL);
	WaitForSingleObject(kth_sem, INFINITE);
        return 0;
}

HANDLE idle_sem;

void linux_exit_idle(void)
{
	ReleaseSemaphore(idle_sem, 1, NULL);
}

HANDLE timer;

void linux_enter_idle(int halted)
{
	HANDLE handles[]={idle_sem, timer};
	int count=sizeof(handles)/sizeof(HANDLE);
	int n;


	n=WaitForMultipleObjects(count, handles, FALSE, halted?0:INFINITE);

	assert(n < WAIT_OBJECT_0+count);

	n-=WAIT_OBJECT_0;

	if (n == 1) {
		linux_trigger_irq(TIMER_IRQ);
	}

	/*
	 * It is OK to exit even if only the timer has expired,
	 * as linux_trigger_irq will trigger an linux_exit_idle anyway
	 */
}

/*
 * With 64 bits, we can cover about 584 years at a nanosecond resolution.
 * Windows counts time from 1601 (do they plan to send a computer back in time
 * and take over the world??) so we neeed to do some substractions, otherwise we
 * would overflow.
 */
static LARGE_INTEGER basetime;

unsigned long long linux_time(void)
{
	SYSTEMTIME st;
	FILETIME ft;
	LARGE_INTEGER li;

	GetSystemTime(&st);
	SystemTimeToFileTime(&st, &ft);
	li.LowPart = ft.dwLowDateTime;
	li.HighPart = ft.dwHighDateTime;


        return (li.QuadPart-basetime.QuadPart)*100;
}

void linux_timer(unsigned long delta)
{
        unsigned long long delta_100ns=delta/100;
	LARGE_INTEGER li = {
		.QuadPart = delta_100ns,
	};

	SetWaitableTimer(timer, &li, 0, NULL, NULL, FALSE);
}

static void (*main_halt)(void);

static void linux_windows_halt(void)
{
	/*
	 * It might take a while to terminate the threads because of the delay
	 * induce by the sync termination procedure. Unfortunatelly there is no
	 * good way of waiting them.
	*/
	while (debug_thread_count != 0)
		Sleep(1);

	if (main_halt)
		main_halt();
}

void threads_init(struct linux_native_operations *lnops)
{
	SYSTEMTIME st;
	FILETIME ft;

	main_halt=lnops->halt;
	lnops->halt=linux_windows_halt;

	lnops->thread_info_alloc=linux_thread_info_alloc;
	lnops->new_thread=linux_new_thread;
	lnops->free_thread=linux_free_thread;
	lnops->context_switch=linux_context_switch;
        kth_sem=CreateSemaphore(NULL, 0, 100, NULL);

	lnops->enter_idle=linux_enter_idle;
	lnops->exit_idle=linux_exit_idle;
        idle_sem=CreateSemaphore(NULL, 0, 100, NULL);

        lnops->time=linux_time;
	GetSystemTime(&st);
	SystemTimeToFileTime(&st, &ft);
	basetime.LowPart = ft.dwLowDateTime;
	basetime.HighPart = ft.dwHighDateTime;
        lnops->timer=linux_timer;
	timer=CreateWaitableTimer(NULL, FALSE, NULL);
}
