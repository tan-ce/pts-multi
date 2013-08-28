## What is pts-multi
pts-multi is a collection of tools for manipulating PTS pseudo terminals in Android which came out of my experimentation after the upgrade to Android 4.3.

The result of these experiments is pts-daemon, which is very similar to other superuser daemons (eg. Superuser or SuperSU). The main difference is that my goal is on users launching root shells with proper terminal handling, while most superuser apps focus is on application running commands as root.

The tools are in a multi-call binary, and includes:

* pts-daemon - A daemon which, if run at startup as root, will allow users to launch and attach any native application to an existing pesudo-terminal. Access is controlled via a password.
* pts-passwd - A utility which allows you to set/change the password used by pts-daemon.
* pts-shell - A frontend which creates a pseudo-terminal, attaches its own standard input/output/error to this terminal, then contacts pts-daemon to attach an application (eg. a busybox shell) to this pseudo-terminal.
* pts-wrap - A small utility which creates a pseudo-terminal, then attaches its own standard input/output/error to this pseudo-terminal.
* pts-exec - A small utility which daemonizes an application then attaches its standard input/output/error to a pseudo-terminal.

## Building the binary
After cloning the git repository,

* $ cd pts-multi/jni
* Open Makefile in an editor and make sure NDK_PATH is set to your NDK directory.
* $ make

Binaries are created in jni/x86_bin/ and update_zip/

Zipping the contents of update_zip should create an Android update.zip which will install the binaries and the daemon.

## Typical Usage
Once the daemon is installed (eg. via the update.zip method), you'll need to run pts-passwd at least once (as root) to set the password.

After that, you may execute

```
pts-shell <path to application>
```

At any time to request the daemon to launch the given application.

## pts-exec and pts-wrap
(a.k.a. non-daemon usage)

In a regular terminal, run pts-wrap. Observe the name of the pseudo-terminal which was created. (eg. /dev/pts/2)

Run pts-exec as root as follows:

```
pts-exec <pseudo-terminal name> <path to application>
```

The application should be launched and attached to the pseudo-terminal.
