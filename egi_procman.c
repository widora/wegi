/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Midas_Zhou
------------------------------------------------------------------*/
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/wait.h>
#include "egi_common.h"
#include "egi_FTsymbol.h"
#include "egi_utils.h"
#include "egi_procman.h"

__attribute__((weak)) const char *app_name="etouch test";

static struct sigaction sigact_cont;
static struct sigaction osigact_cont;  /* Old one */

static struct sigaction sigact_usr;
static struct sigaction osigact_usr;   /* Old one */

/* For common use */
static struct sigaction sigact_common;
static struct sigaction osigact_common;  /* Old one */


#if 0  ////////////////////////////////////////////////////////////////////////
siginfo_t {
               int      si_signo;     /* Signal number */
               int      si_errno;     /* An errno value */
               int      si_code;      /* Signal code */
               int      si_trapno;    /* Trap number that caused
                                         hardware-generated signal
                                         (unused on most architectures) */
               pid_t    si_pid;       /* Sending process ID */
               uid_t    si_uid;       /* Real user ID of sending process */
               int      si_status;    /* Exit value or signal */
               clock_t  si_utime;     /* User time consumed */
               clock_t  si_stime;     /* System time consumed */
               sigval_t si_value;     /* Signal value */
               int      si_int;       /* POSIX.1b signal */
               void    *si_ptr;       /* POSIX.1b signal */
               int      si_overrun;   /* Timer overrun count;
                                         POSIX.1b timers */
               int      si_timerid;   /* Timer ID; POSIX.1b timers */
               void    *si_addr;      /* Memory location which caused fault */
               long     si_band;      /* Band event (was int in
                                         glibc 2.3.2 and earlier) */
               int      si_fd;        /* File descriptor */
               short    si_addr_lsb;  /* Least significant bit of address
                                         (since Linux 2.6.32) */
               void    *si_call_addr; /* Address of system call instruction
                                         (since Linux 3.5) */
               int      si_syscall;   /* Number of attempted system call
                                         (since Linux 3.5) */
               unsigned int si_arch;  /* Architecture of attempted system call
                                         (since Linux 3.5) */
           }
#endif////////////////////////////////////////////////////////////////////////////


/*--------------------------------------------------
Set a signal acition for a common process.

@signum:	Signal number.
@handler:	Handler for the signal

Return:
	0	Ok
	<0	Fail
---------------------------------------------------*/
int egi_common_sigAction(int signum, void(*handler)(int))
{
	sigemptyset(&sigact_common.sa_mask);
	// the signal which triggered the handler will be blocked, unless the SA_NODEFER flag is used.
	//sigact_common.sa_flags|=SA_NODEFER;
	sigact_common.sa_handler=handler;
        if(sigaction(signum, &sigact_common, &osigact_common) <0 ){
                printf("%s: Fail to call sigaction().\n",  __func__);
                return -1;
        }

	return 0;
}



#if 0
/*----------------  For APP  ------------------------------
 Common actions for APP constructor and destructor.

Note: If more than one constructor/destructor funcs exists,
they will be launched one after another.

----------------------------------------------------------*/
void __attribute__((constructor)) app_common_constructor(void)
{
	char log_path[EGI_PATH_MAX];

	printf("APP %s: start  common constructor...\n", app_name);

        /* Set memory allocation option */
        mallopt(M_MMAP_MAX,0);          /* forbid calling mmap to allocate mem */
        mallopt(M_TRIM_THRESHOLD,-1);   /* forbid memory trimming */

        /*  ---  assign signal actions  --- */
        if(egi_assign_AppSigActions() != 0 ) {
                printf("Fail to call egi_assign_AppSigActions() for APP %s.\n", app_name);
                exit(-1);
	}

        /*  ---  Start systick  --- */
        tm_start_egitick();

        /*  ---  EGI General Init Jobs  --- */
	sprintf(log_path,"/mmc/log_%s", app_name);
        if(egi_init_log(log_path) != 0) {
                printf("Fail to init logger for APP %s, quit now.\n", app_name);
                exit(-2);
        }
        if(symbol_load_allpages() !=0 ) {
                printf("Fail to load sym pages for APP %s, quit now.\n", app_name);
		exit(-3);
        }
        if(FTsymbol_load_allpages() !=0 ) {
                printf("Fail to load FTsym pages for APP %s, quit now.\n", app_name);
		exit(-4);
	}

        /* --- FT fonts needs more memory, disable it if not necessary --- */
        if( FTsymbol_load_appfonts() !=0 ) {
                printf("Fail to load FTsym fonts for APP %s, quit now.\n", app_name);
		exit(-5);
	}

        /* --- Init FB device,for sys displaying --- */
        if( init_fbdev(&gv_fb_dev) !=0 ) {
                printf("Fail to initiate FBDEV for APP %s, quit now.\n", app_name);
		exit(-6);
	}

}

void __attribute__((destructor)) app_common_destructor(void)
{
	printf("APP %s: start common destructor...\n", app_name);
	/* ... */
}

#endif


/*----------------  For APP  -------------------------
                Signal handler for SIGCONT

SIGCONT:        To continue the process.
                SIGCONT can't not be blocked.
SIGUSR1:
SIGUSR2:
SIGTERM:        To terminate the process.

-----------------------------------------------------*/
static void app_sigcont_handler( int signum, siginfo_t *info, void *ucont )
{
        pid_t spid=info->si_pid;/* get sender's pid */


        if(signum==SIGCONT) {
                EGI_PLOG(LOGLV_INFO,"%s:[%s] SIGCONT received from process [PID:%d].\n",
                                                                app_name, __func__, spid);

  }
}


/*--------------------  For APP  ------------------------------
           Signal handler for SIGUSR1
SIGUSR1		To stop the process
-------------------------------------------------------------*/
static void app_sigusr_handler( int signum, siginfo_t *info, void *ucont )
{
        pid_t spid=info->si_pid;/* get sender's pid */

  if(signum==SIGUSR1) {
        /* restore FBDEV buffer[0] to FB, do not clear buffer */
        EGI_PLOG(LOGLV_INFO,"%s:[%s] SIGSUR1 received from process [PID:%d].\n", app_name, __func__, spid);

        /* raise SIGSTOP */
        if(raise(SIGSTOP) !=0 ) {
                EGI_PLOG(LOGLV_ERROR,"%s:[%s] Fail to raise(SIGSTOP) to itself.\n",app_name, __func__);
        }
 }

}

/*-------------  For APP  -----------------
    assign signal actions for APP
------------------------------------------*/
int egi_assign_AppSigActions(void)
{
        /* 1. set signal action for SIGCONT */
        sigemptyset(&sigact_cont.sa_mask);
        sigact_cont.sa_flags=SA_SIGINFO; /*  use sa_sigaction instead of sa_handler */
        sigact_cont.sa_flags|=SA_NODEFER; /* Do  not  prevent  the  signal from being received from within its own signal handler */
        sigact_cont.sa_sigaction=app_sigcont_handler;
        if(sigaction(SIGCONT, &sigact_cont, &osigact_cont) <0 ){
                EGI_PLOG(LOGLV_ERROR,"%s:[%s] fail to call sigaction() for SIGCONT.\n", app_name, __func__);
                return -1;
        }

        /* 2. set signal handler for SIGUSR1 */
        sigemptyset(&sigact_usr.sa_mask);
        sigact_usr.sa_flags=SA_SIGINFO; /*  use sa_sigaction instead of sa_handler */
        sigact_usr.sa_flags|=SA_NODEFER; /* Do  not  prevent  the  signal from being received from within its own signal handler */
        sigact_usr.sa_sigaction=app_sigusr_handler;
        if(sigaction(SIGUSR1, &sigact_usr, &osigact_usr) <0 ){
                EGI_PLOG(LOGLV_ERROR,"%s:[%s] fail to call sigaction() for SIGUSR1.\n", app_name, __func__);
                return -2;
        }

        return 0;
}


/*-----------------------------------------------------------
Activate an APP and wait for its state to change.

1. If apid<0 then vfork() and excev() the APP.
2. If apid>0, try to send SIGCONT to activate it.
3. Wait for state change for the apid, that means the parent
   process is hung up until apid state changes(STOP/TERM).

@apid:          PID of the subprocess.
@app_path:      Path to an executiable APP file.

Return:
        <0      Fail to execute or send signal to the APP.
        0       Ok, succeed to execute APP, and get the latest
                changed state.
-------------------------------------------------------------*/
int egi_process_activate_APP(pid_t *apid, char* app_path)
{
        int status;
        int ret;

        EGI_PDEBUG(DBG_PAGE,"------- start check accessibility of the APP --------\n");
        if( access(app_path, X_OK) !=0 ) {
                EGI_PDEBUG(DBG_PAGE,"'%s' is not a recognizble executive file.\n", app_path);
                return -1;
        }

        /* TODO: SIGCHLD handler for exiting child processes
         *      NOTE: STOP/CONTINUE/EXIT of a child process will produce signal SIGCHLD!
         */

        /* 1. If app_ffplay not running, fork() and execv() to launch it. */
        if(*apid<0) {

                EGI_PDEBUG(DBG_PAGE,"----- start fork APP -----\n");
                *apid=vfork();

                /* In APP */
                if(*apid==0) {
                        EGI_PDEBUG(DBG_PAGE,"----- start execv APP -----\n");
                        execv(app_path, NULL);
                        /* Warning!!! if fails! it still holds whole copied context data!!! */
                        EGI_PLOG(LOGLV_ERROR, "%s: fail to execv '%s' after fork(), error:%s",
                                                                __func__, app_path, strerror(errno) );
                        exit(255); /* 8bits(255) will be passed to parent */
                }
                /* In the caller's context */
                else if(*apid <0) {
                        EGI_PLOG(LOGLV_ERROR, "%s: Fail to launch APP '%s'!",__func__, app_path);
                        return -2;
                }
                else {
                        EGI_PLOG(LOGLV_CRITICAL, "%s: APP '%s' launched successfully!\n",
                                                                                __func__, app_path);
                }
        }

        /* 2. Else, assume APP is hungup(stopped), send SIGCONT to activate it. */
        else {
                        EGI_PLOG(LOGLV_CRITICAL, "%s: send SIGCONT to activate '%s'[pid:%d].",
                                                                                __func__, app_path, *apid);
                        if( kill(*apid,SIGCONT)<0 ) {
                                EGI_PLOG(LOGLV_ERROR, "%s: Fail to send SIGCONT to '%s'[pid:%d].",
                                                                                __func__, app_path, *apid);
                                return -3;
                        }
        }

        /* 3. Wait for status of APP to change */
        if(*apid>0) {
                /* wait for status changes,a state change is considered to be:
                 * 1. the child is terminated;
                 * 2. the child is stopped by a signal;
                 * 3. or the child is resumed by a signal.
                 */
                waitpid(*apid, &status, WUNTRACED); /* block |WCONTINUED */

                /* 1. Exit normally */
                if(WIFEXITED(status)){
                        ret=WEXITSTATUS(status);
                        EGI_PLOG(LOGLV_CRITICAL, "%s: APP '%s' exits with ret value: %d.",
                                                                        __func__, app_path, ret);
                        *apid=-1; /* retset */
                }
                /* 2. Terminated by signal, in case APP already terminated. */
                else if( WIFSIGNALED(status) ) {
                        EGI_PLOG(LOGLV_CRITICAL, "%s: APP '%s' terminated by signal number: %d.",
                                                                __func__, app_path, WTERMSIG(status));
                        *apid=-1; /* reset */
                }
                /* 3. Stopped */
                else if(WIFSTOPPED(status)) {
                                EGI_PLOG(LOGLV_CRITICAL, "%s: APP '%s' is just STOPPED!",
                                                                                 __func__, app_path);
                }
                /* 4. Other status */
                else {
                        EGI_PLOG(LOGLV_WARN, "%s: APP '%s' returned with unparsed status=%d",
                                                                        __func__, app_path, status);
                        *apid=-1; /* reset */
                }
        }

        return 0;
}


