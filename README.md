# console-idle

Version 0.1a, December 2020

## What is this? 

`console-idle` is a screen-saver manager for the Linux framebuffer console.

It is a simple utility that provides screen-saver or
standby functionality for Linux framebuffer consoles. Normally, 
when a framebuffer console is in use, only screen blanking is possible,
and even that is not enabled by default on modern systems.  
`Iconsole-idle` allows any custom program, that outputs 
to the framebuffer, to run when the system appears to be idle. 
This can be a screen-saver, or a program that displays 
general information, or anything else.

`console-idle` has a narrow range of applications. It is intended
for minimal and embedded Linux systems that offer a terminal 
interface. It doesn't work with X -- but there's no need for it,
because X has its own screen-saver implementation.  

`console-idle` runs quietly in the background, and monitors selected
input devices for activity. After a certain period of inactivity, it
starts a user-selected screen-saver program, which is expected to 
draw the screen. 
When further activity is detected, `console-idle` restores the
framebuffer and sends a TERM signal to the screen-saver program it launched.

`console-idle` should be compatible with terminal-mode programs 
that use `curses` and similar frameworks to produce complex 
layout. 

`console-idle` has no dependencies, and requires no specific
installation. All configuration is through command-line switches.
`console-idle` It does not read or write any files, and can be
used in a completely read-only Linux system. Its CPU usage is
normally unmeasurable. 

I wrote `console-idle` to use with a console-mode, multi-room audio
controller system based on Raspberry Pi. It works with the 
various framebuffer-based graphics programs in my Github repository. 

## General usage

    console-idle [options] -- command args...

Note the double-dash (--) between the arguments to `console-idle` and
the command to be executed. Without this, arguments to the command will
be interpreted as arguments to `console-idle`.

## Example

    console-idle \\
      --dev /dev/input/event0 --dev=/dev/input/mice \\
      -timeout 60 \\
      -- jpegtofb --sleep 60 --landscape \\
      --randomize /photos/*.jpg 
 
Invoke `jpegtofb` (https://github.com/kevinboone/jpegtofb) 
to provide a slideshow screen-saver whenever the
selected input devices have been idle for more than two minutes.

In this example, `/dev/input/mice` is the standard mouse device, and
`/dev/input/event0` is the keyboard. There's no automated way for 
`console-idle` to determine which devices to monitor -- see
discussion of this point below.

## Building

The usual:

    $ make
    $ sudo make install

`console-idle` has no external dependencies.

## Command-line options

`-d,--device=/dev/..`

An input device such as `/dev/input/event1` or `/dev/input/mice`. Many 
devices can be watched at the same time. The utility generates no
discernable CPU load, however many devices are monitored. 

`-D,--debug`

In debug mode, `console-idle` can be run in the foreground in
a terminal session, and verbose logging can be enabled. Logging is
to `stderr`. Otherwise,
`console-idle` detaches from any controlling terminal, and 
logs to the system logger.

`-f,--fbdev`

Framebuffer device; defaults to `/dev/fb0`. `console-idle` needs
read and write access to the framebuffer, so it can restore the original
screen contents when activity is detected. `console-idle` saves and
restores the framebuffer so that the screen-saver program it launches 
does not have to be able to. 

`-l,--log-level=N`

A number representing the verbosity of logging, from 0 (fatal errors
only) to 4 (function call tracing). Since `console-idle` is not
designed to be run in a user session, low-level (severe) messages are
written to the system log, unless running in debug mode. Consequently,
setting a log level greater than 2 has no effect except in debug 
mode.

`-t,--timeout=seconds`

The time in seconds that console-idle will wait for input activity,
before launching the screen-saver program.  The default is 120 seconds,
or two minutes.

## Permissions issues

`console-idle` requires a huge number of elevated privileges --
on the terminal device, framebuffer device, and input devices. 
It would be very difficult to make it run other than as `root`. 
Most likely the same is true of any screen-saver program it runs,
so 'console-idle' does not attempt to change its user ID when
launching the screen-saver program. 

To run a screen-saver program as an unprivileged user, have
`console-idle` run a shell that uses `su` to switch
users.

## Device selection

Typical devices to monitor include those for the mouse, 
keyboard, and touchscreen. These devices
usually have entries in `/dev/input` that can be polled without stealing
data from other applications. However, there's no systematic way that
the utility can work out which devices to monitor -- this may require
some trial-and-error. Doing `hexdump /dev/input/eventNN` will usually
reveal which devices respond to which kinds of input. It's possible,
and probably necessary, to monitor a number of different devices.

## Limitations

In the end, `console-idle` is a hack. Unlike X, the console has no
API that an application can tap into to coordinate screen-saver
operations. The program does te best it can, but there are
significant limitations.

`console-idle` has no way to know whether the screen-saver
program it is told to launch will work, or even exist. It is the 
user's responsibility to ensure that the path of the screen-saver
program, and any arguments it takes, are correct.

The screen-saver program launched by `console-idle` must run 
_in the foreground_, and be capable of being stopped by the receipt of
a `TERM` signal. 

The screen-saver program must produce no further output to the
framebuffer after the `TERM` signal is received. It should shut
down reasonably quickly -- certainly before the system becomes
idle again. However, so long as it doesn't produce output after being
signalled, it doesn't have to stop immediately.

The screen-saver program need not save or restore the screen contents
`console-idle` will do this. However, `console-idle` 
does not clear the screen when it launches a program -- it assumes
that the screen-saver program will draw the whole screen (or, at least,
blank the unused screen areas).

`console-idle` uses an `ioctl()` call to disable text output from 
the Linux console when it launches the screen-saver program. However,
it can't prevent output from another program that writes directly to the 
framebuffer. Outside of the X environment, most graphical programs
_do_ write directly to the framebuffer, although console-mode
programs rarely do.  The screen-saver program launched
by `console-idle` will try to overwrite whatever happens to be on the
screen, however it was generated. However, if a program is running
that continues to write to the framebuffer in the absence of user
input, then there will be two programs competing for the framebuffer.
The results are likely to be unsightly, at best.

If a console program does produce
complex terminal output in the absence of user input, there's 
always an outside chance
that the screen-saver will switch on or off mid-way through a 
sequence of control characters. Because 
`console-idle` disables terminal character output when the 
screen-saver is running, multi-character sequences could potentially 
be truncated.
The results are likely to be decidedly
odd if this happens, but it can only happen when running a program that
produces output without user input.

When `console-idle` shuts down -- which it will only do when
sent a signal -- it tries to leave the screen as it found it. 
That is, it tries to restore the screen contents if a screen-saver program
is running, and to enable the console cursor if it disabled it.
This process might take a second or so.

`console-idle` can only monitor input devices that work with the
`poll()` system call, and which can have their activity status
reset by reading them. In practice, all Linux raw
input devices with entries in `/dev/input` are of this type. 
Terminal devices like `/dev/tty` are not. 

`console-idle` can't "remove" keyboard or mouse input from the input
buffer -- it monitors raw devices, not any particular terminal's
input buffer. So whatever keystroke or mouse movement caused the 
screen-saver to quit will become available to a program that is waiting
for input. 

`console-idle` only works with 24 bits-per-pixel, linear framebuffers.
The great majority of Linux framebuffers are of this type, but not
all of them.

It should go without saying that `console-idle` will be neither
functional, nor remotely necessary, with an X desktop.

## Notes

`console-idle` uses `execvp()` to launch the screen-saver
program. This means that the specified program will be searched 
for in the usual
locations and the directories specified by `$PATH`. The screen-saver
program will have the same environment as the `console-idle` program. 

To disable output from the console terminal, `console-idle` uses
an ioctl() call to set the terminal to "graphics" mode. "Ordinary"
text output is completely ignored in "graphics" mode, and the regular
console cursor is hidden. 

To restore the screen contents after shutting down the screen-saver 
program, `console-idle` stores the original framebuffer contents in
RAM. With a large display, this could use a significant amount of
memory. However, `console-idle` is designed to be usable on embedded
Linux systems with a read-only filesystem, so storage in a file
might be impossible. On a Raspberry Pi with an 800x480 display, for example,
storing the screen contents uses just under 1Mb of RAM. 

`console-idle` can be started at any point in the initialization of
the system after the `/dev/` filesystem is available. Because of the
wide variety of different embedded system configurations, it's difficult
to give more specific installation advice than this.

Because there's only so many hours in the day, `console-idle` uses
a general-purpose library that I use in most of my utilities. There's
far more code in the source bundle than is actually required -- that's
the way of libraries. The `Makefile` enables segment garbage collection,
so that unused library functions are not included in the final executable,
which should be about 30kB in size.

## Legal, etc

`console-idle` is copyright (c)2020 Kevin Boone, and distributed under the
terms of the GNU Public Licence, version 3.0. Essentially that means
you may use the software however you wish, so long as the source
code continues to be made available, and the original author is
acknowledged. There is no warranty of any kind.

Framebuffer memory

## Revision history

0.1a December 2020<br/>
First working release

