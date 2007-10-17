#include <pthread.h>
#include <malloc.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <assert.h>
#include <unistd.h>

#include "include/asm/callbacks.h"

struct _thread_info {
        pthread_t th;
        pthread_mutex_t sched_mutex;
	int dead;
};

struct kernel_thread_helper_arg {
        int (*fn)(void*);
        void *arg;
        struct _thread_info *pti;
};

static int debug_thread_count=0;

void* linux_thread_info_alloc(void)
{
        struct _thread_info *pti=malloc(sizeof(*pti));

	assert(pti != NULL);

        pthread_mutex_init(&pti->sched_mutex, NULL);
        pthread_mutex_lock(&pti->sched_mutex);
	pti->dead=0;

	return pti;
}

void linux_context_switch(void *prev, void *next)
{
        struct _thread_info *_prev=(struct _thread_info*)prev;
        struct _thread_info *_next=(struct _thread_info*)next;

        pthread_mutex_unlock(&_next->sched_mutex);
        pthread_mutex_lock(&_prev->sched_mutex);
	if (_prev->dead) {
		free(_prev);
		debug_thread_count--;
		pthread_exit(NULL);
	}
}

pthread_mutex_t kth_mutex = PTHREAD_MUTEX_INITIALIZER;

void* kernel_thread_helper(void *arg)
{
        struct kernel_thread_helper_arg *ktha=(struct kernel_thread_helper_arg*)arg;
        int (*fn)(void*)=ktha->fn;
        void *farg=ktha->arg;
        struct _thread_info *pti=ktha->pti;

        pthread_mutex_unlock(&kth_mutex);
        pthread_mutex_lock(&pti->sched_mutex);
        return (void*)fn(farg);
}

void linux_free_thread(void *arg)
{
        struct _thread_info *pti=(struct _thread_info*)arg;

	pti->dead=1;
        pthread_mutex_unlock(&pti->sched_mutex);
}

int linux_new_thread(int (*fn)(void*), void *arg, void *pti)
{
        struct kernel_thread_helper_arg ktha = {
                .fn = fn,
                .arg = arg,
                .pti = (struct _thread_info*)pti
        };
        int rc;

	debug_thread_count++;
        rc=pthread_create(&ktha.pti->th, NULL, kernel_thread_helper, &ktha);
        pthread_mutex_lock(&kth_mutex);
        return rc;
}

typedef struct {
	pthread_mutex_t lock;
	int count;
	pthread_cond_t cond;
} pthread_sem_t;

static pthread_sem_t idle_sem = {
	.lock = PTHREAD_MUTEX_INITIALIZER,
	.count = 0,
	.cond = PTHREAD_COND_INITIALIZER
};

void linux_exit_idle(void)
{
	pthread_mutex_lock(&idle_sem.lock);
	idle_sem.count++;
	if (idle_sem.count > 0)
		pthread_cond_signal(&idle_sem.cond);
	pthread_mutex_unlock(&idle_sem.lock);
}

void linux_enter_idle(int halted)
{
	sigset_t sigmask;

	if (halted)
		return;

	/*
	 * Avoid deadlocks by blocking signals for idle thread. A few notes:
	 *
	 * - POSIX says it will send the signal to one of the threads which has
	 * signal unblocked
	 * - we have at least 2 running threads: idle and init
	 * - only init can call this function, thus only init can block the signal
	 *
	 */
	sigemptyset(&sigmask);
	sigaddset(&sigmask, SIGALRM);

	pthread_sigmask(SIG_BLOCK, &sigmask, NULL);
	pthread_mutex_lock(&idle_sem.lock);
	while (idle_sem.count <= 0)
		pthread_cond_wait(&idle_sem.cond, &idle_sem.lock);
	idle_sem.count--;
	pthread_mutex_unlock(&idle_sem.lock);
	pthread_sigmask(SIG_UNBLOCK, &sigmask, NULL);
}

unsigned long long linux_time(void)
{
        struct timeval tv;

        gettimeofday(&tv, NULL);

        return tv.tv_sec*1000000000ULL+tv.tv_usec*1000ULL;
}

void sigalrm(int sig)
{
        linux_trigger_irq(TIMER_IRQ);
}

void linux_timer(unsigned long delta)
{
        unsigned long long delta_us=delta/1000;
        struct timeval tv = {
                .tv_sec = delta_us/1000000,
                .tv_usec = delta_us%1000000
        };
        struct itimerval itval = {
                .it_interval = {0, },
                .it_value = tv
        };

        setitimer(ITIMER_REAL, &itval, NULL);
}

static void (*main_halt)(void);

static void linux_posix_halt(void)
{
	/*
	 * It might take a while to terminate the threads because of the delay
	 * induce by the sync termination procedure. Unfortunatelly there is no
	 * good way of waiting for them.
	 */
	while (debug_thread_count != 0)
		sleep(1);
	if (main_halt)
		main_halt();
}

void threads_init(struct linux_native_operations *lnops)
{
	main_halt=lnops->halt;
	lnops->halt=linux_posix_halt;

	lnops->thread_info_alloc=linux_thread_info_alloc;
	lnops->new_thread=linux_new_thread;
	lnops->free_thread=linux_free_thread;
	lnops->context_switch=linux_context_switch;
        pthread_mutex_lock(&kth_mutex);

	lnops->enter_idle=linux_enter_idle;
	lnops->exit_idle=linux_exit_idle;

        lnops->time=linux_time;
        lnops->timer=linux_timer;
        signal(SIGALRM, sigalrm);


}

