#include <apr.h>
#include <apr_pools.h>
#include <apr_thread_proc.h>
#include <apr_thread_mutex.h>
#include <apr_thread_cond.h>
#include <apr_file_io.h>
#include <apr_time.h>
#include <apr_poll.h>

#include <malloc.h>
#include <assert.h>

#include "include/asm/callbacks.h"
#include "lklops.h"

static apr_pool_t *pool;

struct _thread_info {
        apr_thread_t *thread;
        apr_thread_mutex_t *sched_mutex;
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

        apr_thread_mutex_create(&pti->sched_mutex, APR_THREAD_MUTEX_UNNESTED, pool);
        apr_thread_mutex_lock(pti->sched_mutex);
	pti->dead=0;

	return pti;
}

void linux_context_switch(void *prev, void *next)
{
        struct _thread_info *_prev=(struct _thread_info*)prev;
        struct _thread_info *_next=(struct _thread_info*)next;

        apr_thread_mutex_unlock(_next->sched_mutex);
        apr_thread_mutex_lock(_prev->sched_mutex);
	if (_prev->dead) {
		apr_thread_t *thread=_prev->thread;
		apr_thread_mutex_destroy(_prev->sched_mutex);
		free(_prev);
		debug_thread_count--;
		apr_thread_exit(thread, 0);
	}
}

apr_thread_mutex_t *kth_mutex;

void* APR_THREAD_FUNC kernel_thread_helper(apr_thread_t *thr, void *arg)
{
        struct kernel_thread_helper_arg *ktha=(struct kernel_thread_helper_arg*)arg;
        int (*fn)(void*)=ktha->fn;
        void *farg=ktha->arg;
        struct _thread_info *pti=ktha->pti;

        apr_thread_mutex_unlock(kth_mutex);
        apr_thread_mutex_lock(pti->sched_mutex);
        return (void*)fn(farg);
}

void linux_free_thread(void *arg)
{
        struct _thread_info *pti=(struct _thread_info*)arg;

	pti->dead=1;
        apr_thread_mutex_unlock(pti->sched_mutex);
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
        rc=apr_thread_create(&ktha.pti->thread, NULL, &kernel_thread_helper, &ktha, pool);
        apr_thread_mutex_lock(kth_mutex);
        return rc;
}

typedef struct {
	apr_thread_mutex_t *lock;
	int count;
	apr_thread_cond_t *cond;
} apr_sem_t;

unsigned long long linux_time(void)
{
	return apr_time_now()*1000;
}


/*
 * APR does not provide timers -- he reason for this ugly hack.
 */
static unsigned long long timer_exp;
static apr_file_t *events_pipe_in, *events_pipe_out;
static apr_pollset_t *pollset;

void linux_timer(unsigned long delta)
{
	if (delta)
		timer_exp=linux_time()+delta;
	else
		timer_exp=0;
}

void linux_exit_idle(void)
{
	char c;
	apr_size_t n=1;
	
	apr_file_write(events_pipe_out, &c, &n);
}

void linux_enter_idle(int halted)
{
	signed long long delta=timer_exp-linux_time();
	apr_int32_t num=0;
	const apr_pollfd_t *descriptors;

	if (!timer_exp && !halted)
		apr_pollset_poll(pollset, 0, &num, &descriptors);
	else {
		if (delta > 0 && !halted) {
			apr_pollset_poll(pollset, delta/1000, &num, &descriptors);
		}
	}

	if (num > 0) {
		char c;
		apr_size_t n=1;
		apr_file_read(events_pipe_in, &c, &n);
	}

	if (timer_exp <= linux_time()) {
		timer_exp=0;
		linux_trigger_irq(TIMER_IRQ);
	}
}

long linux_panic_blink(long time)
{
	assert(0);
	return 0;
}

static void *_phys_mem;

void linux_mem_init(unsigned long *phys_mem, unsigned long *phys_mem_size)
{
	*phys_mem_size=16*1024*1024;
	*phys_mem=(unsigned long)malloc(*phys_mem_size);
}

void linux_halt(void)
{
	free(_phys_mem);
}

static struct linux_native_operations lnops = {
	.panic_blink = linux_panic_blink,
	.mem_init = linux_mem_init,
	.halt = linux_halt,
	.thread_info_alloc = linux_thread_info_alloc,
	.new_thread = linux_new_thread,
	.free_thread = linux_free_thread,
	.context_switch = linux_context_switch,
	.enter_idle = linux_enter_idle,
	.exit_idle = linux_exit_idle,
	.time = linux_time,
	.timer = linux_timer
};

static apr_thread_t *init;

void* APR_THREAD_FUNC init_thread(apr_thread_t *thr, void *arg)
{
	linux_start_kernel(&lnops, "");
	return NULL;
}

void lkl_init(int (*init_2)(void))
{
	lnops.init=init_2;

	apr_pool_create(&pool, NULL);

	apr_thread_mutex_create(&kth_mutex, APR_THREAD_MUTEX_UNNESTED, pool);
        apr_thread_mutex_lock(kth_mutex);

	apr_pollset_create(&pollset, 1, pool, 0);
	apr_file_pipe_create(&events_pipe_in, &events_pipe_out, pool);
	apr_pollfd_t apfd = {
		.p = pool,
		.desc_type = APR_POLL_FILE,
		.reqevents = APR_POLLIN,
		.desc = {
			.f = events_pipe_in
		}
	};
	apr_pollset_add(pollset, &apfd);

	apr_thread_create(&init, NULL, init_thread, NULL, pool);
}

extern long wrapper_sys_halt();
extern long wrapper_sys_sync();
extern long wrapper_sys_umount(const char*, int);


void lkl_fini(unsigned int flag)
{
	apr_status_t ret;
	
	if(0 == (flag & LKL_FINI_DONT_UMOUNT_ROOT))
		wrapper_sys_umount("/", 0);
	wrapper_sys_halt();
	
	apr_thread_join(&ret, init);
}
