/*============================================================================
  
  console-idle
  
  console_idle_main.c

  Copyright (c)1990-2020 Kevin Boone. Distributed under the terms of the
  GNU Public Licence, v3.0

  ==========================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <errno.h> 
#include <getopt.h> 
#include <math.h> 
#include <unistd.h> 
#include <syslog.h> 
#include <poll.h> 
#include <fcntl.h> 
#include <ctype.h> 
#include <signal.h> 
#include <pwd.h> 
#include <linux/kd.h> 
#include <sys/ioctl.h> 
#include <klib/klib.h> 

#define KLOG_CLASS "console_idle.main"

#define MAX_DEVS 32
#define DEFAULT_TIMEOUT 120
#define DEFAULT_FBDEV "/dev/fb0" 

BOOL stop = FALSE;

typedef struct _LogContext
  {
  BOOL debug;
  } LogContext;

/*============================================================================
  
  console_idle_log_handler

  ==========================================================================*/
void console_idle_log_handler (KLogLevel level, const char *cls, 
                  void *user_data, const char *msg)
  {
  LogContext *log_context = (LogContext *)user_data;
  if (log_context->debug)
    {
    fprintf (stderr, "%s %s: %s\n", klog_level_to_utf8 (level), cls, msg);
    }
  else
    {
    if (level < 3) 
      {
      openlog (NAME, 0, LOG_USER);
      switch (level)
	{
	case KLOG_ERROR:
	  syslog (LOG_ERR, "%s", msg);
	  break;
	case KLOG_WARN:
	  syslog (LOG_WARNING, "%s", msg);
	  break;
	case KLOG_INFO:
	  syslog (LOG_INFO, "%s", msg);
	  break;
	default:; // Should not get here
	}
      closelog ();
      }
    }
  }

/*============================================================================
  
  console_idle_show_usage 

  ==========================================================================*/
void console_idle_show_usage (const char *argv0, FILE *f) 
  {
  fprintf (f, "Usage: %s [options] -- command args...\n", argv0);
  fprintf (f, "     -d,--device=/dev/...   input device to monitor\n");
  fprintf (f, "     -D,--debug             run in debug mode\n");
  fprintf (f, "     -f,--fbdev=/dev/...    framebuffer device (/dev/fb0)\n");
  fprintf (f, "     -l,--log-level=N       log verbosity, 0-4\n");
  fprintf (f, "     -t,--timeout=seconds   seconds to idle (120)\n");
  fprintf (f, "Multiple input devices may be specified.\n");
  }

/*============================================================================
  
  console_idle_show_version

  ==========================================================================*/
void console_idle_show_version (void) 
  {
  printf (NAME " version " VERSION "\n");
  printf ("Copyright (c)2020 Kevin Boone\n");
  printf 
    ("Distributed according to the terms of the GNU Public Licence, v3.0\n");
  }

/*============================================================================
  
  console_idle_init_fdset

  ==========================================================================*/
BOOL console_idle_init_fdset (int ndevs, char* const* devs, 
       struct pollfd *fdset)
  {
  KLOG_IN
  BOOL ret = FALSE;

  klog_debug (KLOG_CLASS, "Opening file descriptors");

  int ndev = 0;

  for (int i = 0; i < ndevs; i++)
    {
    const char *device = devs[i]; 

    klog_debug (KLOG_CLASS, "Opening device %s", device);
    int fd = open (device, O_RDONLY);
    if (fd >= 0)
      {
      fdset[ndev].fd = fd;
      fdset[ndev].events = POLLIN;
      ndev++;
      }
    else
      {
      klog_error (KLOG_CLASS, "Can't open device %s: %s", 
         device, strerror (errno));
      }
    }

  if (ndev == ndevs) ret = TRUE;

  KLOG_OUT
  return ret;
  }

/*============================================================================
  
  console_idle_close_fdset

  ==========================================================================*/
void console_idle_close_fdset (int ndevs, struct pollfd *fdset)
  {
  KLOG_IN

  klog_debug (KLOG_CLASS, "Closing file descriptors");
  for (int i = 0; i < ndevs; i++)
      close (fdset[i].fd);

  KLOG_OUT
  }

/*============================================================================
  
  console_idle_wait_for_idle

  ==========================================================================*/
void console_idle_wait_for_idle (int ndevs, const struct pollfd *fdset_base, 
        int timeout)
  {
  KLOG_IN

  struct pollfd fdset [MAX_DEVS];

  int ticks = 0;
  BOOL idle = FALSE;
  klog_debug (KLOG_CLASS, "Waiting for %d second timeout", timeout); 
  while (!idle && !stop)
    {
    memcpy (&fdset, fdset_base, sizeof (fdset));
    int p = poll (fdset, ndevs, 1000);
    if (p <= 0) 
      klog_debug (KLOG_CLASS, "poll() timed out");
    for (int i = 0; i < ndevs; i++)
      {
      if (fdset[i].revents & POLLIN)
	{
	char buff[256];
	/* int n = */ read (fdset[i].fd, buff, sizeof (buff));
	//klog_debug (KLOG_CLASS, "Read %d from %s", n, devs[i]);
	klog_debug (KLOG_CLASS, "Resetting timeout");
	ticks = 0;
	}
      }
    ticks++;
    if (ticks >= timeout) idle = TRUE;
    }

  KLOG_OUT
  }

/*============================================================================
  
  console_idle_wait_for_active

  ==========================================================================*/
void console_idle_wait_for_active (int ndevs, const struct pollfd *fdset_base, 
        int timeout)
  {
  KLOG_IN

  struct pollfd fdset [MAX_DEVS];

  BOOL idle = TRUE;
  klog_debug (KLOG_CLASS, "Waiting for %d second timeout", timeout); 
  while (idle && !stop)
    {
    memcpy (&fdset, fdset_base, sizeof (fdset));
    int p = poll (fdset, ndevs, 1000);
    if (p <= 0) 
      klog_debug (KLOG_CLASS, "poll() timed out");
    for (int i = 0; i < ndevs; i++)
      {
      if (fdset[i].revents & POLLIN)
	{
	char buff[256];
	/* int n = */ read (fdset[i].fd, buff, sizeof (buff));
	//klog_debug (KLOG_CLASS, "Read %d from %s", n, devs[i]);
	klog_debug (KLOG_CLASS, "Resetting timeout");
        idle = FALSE;
	}
      }
    }

  KLOG_OUT
  }

/*============================================================================
  
  console_idle_exec_prog

  ==========================================================================*/
int console_idle_exec_prog (int argc, char * const* argv)
  {
  KLOG_IN
  int pid;

  signal (SIGCHLD, SIG_IGN);

  klog_debug (KLOG_CLASS, "Executing command %s", argv[0]);

  pid = fork(); 
  if (pid == 0)
    {
    // Child
    execvp (argv[0], argv); 
    // We should never get here
    klog_error (KLOG_CLASS, "Can't execute %s: %s\n", 
       argv[0], strerror (errno));
    } 
  else if (pid > 0)
    {
    // parent
    }
  else
    {
    // Error
    }

  KLOG_OUT
  return pid;
  }

/*============================================================================
  
  console_idle_save_framebuffer

  ==========================================================================*/
void console_init_save_framebuffer (FrameBuffer *fb, BitmapRGB *fb_save)
  {
  framebuffer_init (fb, NULL);
  bitmaprgb_from_fb (fb_save, fb, 0, 0);
  framebuffer_deinit (fb);
  }

/*============================================================================
  
  console_idle_restore_framebuffer

  ==========================================================================*/
void console_init_restore_framebuffer (FrameBuffer *fb, 
        BitmapRGB *fb_save)
  {
  framebuffer_init (fb, NULL);
  bitmaprgb_to_fb (fb_save, fb, 0, 0);
  framebuffer_deinit (fb);
  }

/*============================================================================
  
  console_idle_hide_cursor

  ==========================================================================*/
void console_init_hide_cursor (void)
  {
  klog_debug (KLOG_CLASS, "Hide cursor");
  int f = open ("/dev/tty0", O_WRONLY);
  if (f >= 0)
    {
    ioctl (f, KDSETMODE, 1);
    close (f);
    }
  else
    klog_warn (KLOG_CLASS, "Can't open /dev/tty0");
  }

/*============================================================================
  
  console_idle_show_cursor

  ==========================================================================*/
void console_init_show_cursor (void)
  {
  klog_debug (KLOG_CLASS, "Show cursor");
  int f = open ("/dev/tty0", O_WRONLY);
  if (f >= 0)
    {
    ioctl (f, KDSETMODE, 0);
    close (f);
    }
  else
    klog_warn (KLOG_CLASS, "Can't open /dev/tty0");
  }

/*============================================================================
  
  console_idle_main_loop

  timeout --length of time to allow console to be idle
  devs -- array of devices to monitor for intput
  ndevs -- size of devs array

  ==========================================================================*/
void console_idle_main_loop (int timeout, int ndevs, char* const* devs,
       int argc, char * const* argv, FrameBuffer *fb, BitmapRGB *fb_save)
  {
  KLOG_IN
  struct pollfd fdset_base [MAX_DEVS];
  memset (fdset_base, 0, sizeof (fdset_base));
  if (console_idle_init_fdset (ndevs, devs, fdset_base))
    {
    console_idle_close_fdset (ndevs, fdset_base);
    stop = FALSE;

     while (!stop)
      {
      console_idle_init_fdset (ndevs, devs, fdset_base);
      console_idle_wait_for_idle (ndevs, fdset_base, timeout);
      console_idle_close_fdset (ndevs, fdset_base);

      // Save framebuffer 
      console_init_hide_cursor ();
      console_init_save_framebuffer (fb, fb_save);
      int pid = console_idle_exec_prog (argc, argv); 
      klog_debug (KLOG_CLASS, "PID is %d", pid);    

      console_idle_init_fdset (ndevs, devs, fdset_base);
      console_idle_wait_for_active (ndevs, fdset_base, timeout);
      console_idle_close_fdset (ndevs, fdset_base);

      // Kill child process
      kill (pid, SIGTERM);

      // Restore framebuffer 
      console_init_restore_framebuffer (fb, fb_save);
      console_init_show_cursor ();
      }
    }
  KLOG_OUT
  } 

/*============================================================================
  
  console_idle_quit

  ==========================================================================*/
void console_idle_quit (int dummy)
  {
  klog_info (KLOG_CLASS, "Shutting down on signal");
  stop = TRUE;
  }

/*============================================================================
  
  console_idle_main 

  ==========================================================================*/
int console_idle_main (int argc, char **argv)
  {
  KLOG_IN
  int ret = 0;

  klog_init (KLOG_WARN, NULL, NULL);
  klog_info (KLOG_CLASS, "Starting");

  BOOL show_version = FALSE;
  BOOL show_usage = FALSE;
  BOOL debug = FALSE;
  char *devs [MAX_DEVS];
  int ndev_in = 0;
  int timeout = DEFAULT_TIMEOUT;
  char *fbdev = NULL;

  int log_level = KLOG_WARN;

  static struct option long_options[] =
    {
      {"help", no_argument, NULL, 'h'},
      {"version", no_argument, NULL, 'v'},
      {"device", required_argument, NULL, 'd'},
      {"debug", no_argument, NULL, 'D'},
      {"fbdev", required_argument, NULL, 'f'},
      {"log-level", required_argument, NULL, 'l'},
      {"timeout", required_argument, NULL, 't'},
      {0, 0, 0, 0}
    };

   int opt;
   ret = 0;
   while (ret == 0)
     {
     int option_index = 0;
     opt = getopt_long (argc, argv, "vhdl:d:t:f:D",
     long_options, &option_index);

     if (opt == -1) break;

     switch (opt)
       {
       case 'D':
        debug = TRUE; break;
       case 'f': 
         fbdev = strdup (optarg); break;
       case 'h': case '?': 
         show_usage = TRUE; break;
       case 'v': 
         show_version = TRUE; break;
       case 'l':
         log_level = atoi (optarg); break;
       case 't':
         timeout = atoi (optarg); break;
       case 'd':
         if (ndev_in < MAX_DEVS - 1)
           {
           devs[ndev_in] = strdup (optarg);
           ndev_in++;
           }
         break;
       default:
           ret = EINVAL;
       }
    }
  
  if (show_version)
    {
    console_idle_show_version(); 
    ret = -1;
    }

  if (ret == 0 && show_usage)
    {
    console_idle_show_usage (argv[0], stdout); 
    ret = -1;
    }
  
  if (ret == 0 && ndev_in == 0)
    {
    klog_error (KLOG_CLASS, 
      "No input devices were specified. Use --device=xxx...");
    ret = -1;
    }

  LogContext log_context;
  log_context.debug = debug;
  klog_init (log_level, console_idle_log_handler, &log_context);

  int new_argc = argc - optind; 
  char **new_argv = malloc ((new_argc + 1) * sizeof (char *));
  for (int i = 0; i < new_argc; i++)
    {
    new_argv [i] = strdup (argv[optind + i]);
    }
  new_argv[new_argc] = NULL;   

  if (fbdev == NULL)
    {
    fbdev = strdup (DEFAULT_FBDEV);
    klog_info (KLOG_CLASS, "Using default framebuffer %s", fbdev);
    }

  FrameBuffer *fb = framebuffer_create (fbdev);
  char *error = NULL;
  int fb_w, fb_h;
  if (framebuffer_init (fb, &error))
    {
    klog_debug (KLOG_CLASS, "Framebuffer initialization OK");
    fb_w = framebuffer_get_width (fb);
    fb_h = framebuffer_get_width (fb);
    }
  else
    {
    ret = -1;
    free (error);
    }

  if (ret == 0)
    {
    BitmapRGB *fb_save = bitmaprgb_create (fb_w, fb_h); 

    signal (SIGQUIT, console_idle_quit);
    signal (SIGTERM, console_idle_quit);
    signal (SIGHUP, console_idle_quit);
    signal (SIGINT, console_idle_quit);

    if (!debug)
      daemon (0, 0);

    console_idle_main_loop (timeout, ndev_in, devs, new_argc, new_argv, 
             fb, fb_save);
    bitmaprgb_destroy (fb_save);
    }
  
  for (int i = 0; i < new_argc; i++)
    free (new_argv[i]);
  free (new_argv);

  for (int i = 0; i < ndev_in; i++)
    free (devs[i]);

  if (fbdev) free (fbdev);

  framebuffer_destroy (fb);

  klog_debug (KLOG_CLASS, "Done");
  KLOG_OUT
  return ret;
  }


