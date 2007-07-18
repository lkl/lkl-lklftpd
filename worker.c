// main FTP protocol loop handling
#include <apr_network_io.h>
#include <apr_errno.h>

#include "worker.h"
#include "config.h"
#include "utils.h"
#include "ftpcodes.h"
#include "cmdio.h"
#include "sess.h"


static void emit_greeting(struct lfd_sess * p_sess)
{
	if(0 != strlen(lfd_config_banner_string))
	{
		lfd_cmdio_write(p_sess, FTP_GREET, lfd_config_banner_string);
	}
}
/*
static void
parse_username_password(struct lfd_sess* p_sess)
{
  while (1)
  {
    vsf_cmdio_get_cmd_and_arg(p_sess, &p_sess->ftp_cmd_str,
                              &p_sess->ftp_arg_str, 1);
    if (str_equal_text(&p_sess->ftp_cmd_str, "USER"))
    {
      handle_user_command(p_sess);
    }
    else if (str_equal_text(&p_sess->ftp_cmd_str, "PASS"))
    {
      handle_pass_command(p_sess);
    }
    else if (str_equal_text(&p_sess->ftp_cmd_str, "QUIT"))
    {
      vsf_cmdio_write(p_sess, FTP_GOODBYE, "Goodbye.");
      vsf_sysutil_exit(0);
    }
    else if (str_equal_text(&p_sess->ftp_cmd_str, "FEAT"))
    {
      handle_feat(p_sess);
    }
    else if (tunable_ssl_enable && str_equal_text(&p_sess->ftp_cmd_str, "AUTH"))
    {
      handle_auth(p_sess);
    }
    else if (tunable_ssl_enable && str_equal_text(&p_sess->ftp_cmd_str, "PBSZ"))
    {
      handle_pbsz(p_sess);
    }
    else if (tunable_ssl_enable && str_equal_text(&p_sess->ftp_cmd_str, "PROT"))
    {
      handle_prot(p_sess);
    }
    else
    {
      vsf_cmdio_write(p_sess, FTP_LOGINERR,
                      "Please login with USER and PASS.");
    }
  }
}
*/

void * lfd_worker_protocol_main(apr_thread_t * thd, void* param)
{
	apr_status_t	rc;
	apr_socket_t 	* sock = (apr_socket_t*) param;
	struct lfd_sess * sess;
	rc = lfd_sess_create(&sess, thd, sock);
	if(APR_SUCCESS != rc)
	{

	}
	emit_greeting(sess);
	//parse_username_password(p_sess);

	printf("hello\n");
	lfd_sess_destroy(sess);
	return NULL;
}

