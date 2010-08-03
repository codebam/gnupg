/* init.c - Various initializations
 *	Copyright (C) 2007 Free Software Foundation, Inc.
 *
 * This file is part of GnuPG.
 *
 * GnuPG is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * GnuPG is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#ifdef WITHOUT_GNU_PTH /* Give the Makefile a chance to build without Pth.  */
#undef HAVE_PTH
#undef USE_GNU_PTH
#endif

#ifdef HAVE_W32_SYSTEM
#include <windows.h>
#endif
#ifdef HAVE_PTH      
#include <pth.h>
#endif
#ifdef HAVE_W32CE_SYSTEM
# include <assuan.h> /* For _assuan_w32ce_finish_pipe. */
#endif

#include "util.h"


#ifdef HAVE_W32CE_SYSTEM
static void parse_std_file_handles (int *argcp, char ***argvp);
static void
sleep_on_exit (void)
{
  /* The sshd on CE swallows some of the command output.  Sleeping a
     while usually helps.  */
  Sleep (400);
}
#endif /*HAVE_W32CE_SYSTEM*/


/* If STRING is not NULL write string to es_stdout or es_stderr.  MODE
   must be 1 or 2.  If STRING is NULL flush the respective stream.  */
static int
writestring_via_estream (int mode, const char *string)
{
  if (mode == 1 || mode == 2)
    {
      if (string)
        return es_fputs (string, mode == 1? es_stdout : es_stderr);
      else
        return es_fflush (mode == 1? es_stdout : es_stderr);
    }
  else
    return -1;
}


/* This function is to be used early at program startup to make sure
   that some subsystems are initialized.  This is in particular
   important for W32 to initialize the sockets so that our socket
   emulation code used directly as well as in libassuan may be used.
   It should best be called before any I/O is done so that setup
   required for logging is ready.  ARGCP and ARGVP are the addresses
   of the parameters given to main.  This function may modify them.

   CAUTION: This might be called while running suid(root).  */
void
init_common_subsystems (int *argcp, char ***argvp)
{
  /* Try to auto set the character set.  */
  set_native_charset (NULL); 

#ifdef HAVE_W32_SYSTEM
  /* For W32 we need to initialize the socket layer.  This is because
     we use recv and send in libassuan as well as at some other
     places.  If we are building with PTH we let pth_init do it.  We
     can't do much on error so we ignore them.  An error would anyway
     later pop up if one of the socket functions is used. */
# ifdef HAVE_PTH
  pth_init ();
# else
  {
    WSADATA wsadat;
    
    WSAStartup (0x202, &wsadat);
  }
# endif /*!HAVE_PTH*/
#endif
  
#ifdef HAVE_W32CE_SYSTEM
  /* Register the sleep exit function before the estream init so that
     the sleep will be called after the estream registered atexit
     function which flushes the left open estream streams and in
     particular es_stdout.  */
  atexit (sleep_on_exit);
#endif

  /* Initialize the Estream library. */
  es_init ();

  /* Special hack for Windows CE: We extract some options from arg
     to setup the standard handles.  */
#ifdef HAVE_W32CE_SYSTEM
  parse_std_file_handles (argcp, argvp);
#else
  (void)argcp;
  (void)argvp;
#endif

  /* --version et al shall use estream as well.  */
  argparse_register_outfnc (writestring_via_estream);

}



/* WindowsCE uses a very strange way of handling the standard streams.
   There is a function SetStdioPath to associate a standard stream
   with a file or a device but what we really want is to use pipes as
   standard streams.  Despite that we implement pipes using a device,
   we would have some limitations on the number of open pipes due to
   the 3 character limit of device file name.  Thus we don't take this
   path.  Another option would be to install a file system driver with
   support for pipes; this would allow us to get rid of the device
   name length limitation.  However, with GnuPG we can get away be
   redefining the standard streams and passing the handles to be used
   on the command line.  This has also the advantage that it makes
   creating a process much easier and does not require the
   SetStdioPath set and restore game.  The caller needs to pass the
   rendezvous ids using up to three options:

     -&S0=<rvid> -&S1=<rvid> -&S2=<rvid>

   They are all optional but they must be the first arguments on the
   command line.  Parsing stops as soon as an invalid option is found.
   These rendezvous ids are then used to finish the pipe creation.*/
#ifdef HAVE_W32CE_SYSTEM
static void
parse_std_file_handles (int *argcp, char ***argvp)
{
  int argc = *argcp;
  char **argv = *argvp;
  const char *s;
  assuan_fd_t fd;
  int i;
  int fixup = 0;

  if (!argc)
    return;

  for (argc--, argv++; argc; argc--, argv++)
    {
      s = *argv;
      if (*s == '-' && s[1] == '&' && s[2] == 'S'
          && (s[3] == '0' || s[3] == '1' || s[3] == '2')
          && s[4] == '=' 
          && (strchr ("-01234567890", s[5]) || !strcmp (s+5, "null")))
        {
          if (s[5] == 'n')
            fd = ASSUAN_INVALID_FD;
          else
            fd = _assuan_w32ce_finish_pipe (atoi (s+5), s[3] != '0');
          _es_set_std_fd (s[3] - '0', (int)fd);
          fixup++;
        }
      else
        break;
    }

  if (fixup)
    {
      argc = *argcp;
      argc -= fixup;
      *argcp = argc;

      argv = *argvp;
      for (i=1; i < argc; i++)
        argv[i] = argv[i + fixup];
      for (; i < argc + fixup; i++)
        argv[i] = NULL;
    }


}
#endif /*HAVE_W32CE_SYSTEM*/