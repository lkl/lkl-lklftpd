#ifndef LKLFTPD_WORKER_H__
#define LKLFTPD_WORKER_H__

#include <apr_errno.h>
#include <apr_thread_proc.h>


void * APR_THREAD_FUNC lfd_worker_protocol_main( apr_thread_t * thd, void* param);
#endif//LKLFTPD_WORKER_H__
