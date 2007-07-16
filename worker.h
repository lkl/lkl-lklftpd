#ifndef LKLFTPD_WORKER_H__
#define LKLFTPD_WORKER_H__

#include <apr_thread_proc.h>

void * lfd_worker_protocol_main( apr_thread_t * thd, void* param);
#endif//LKLFTPD_WORKER_H__
