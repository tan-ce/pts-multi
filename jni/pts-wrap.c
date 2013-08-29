/*
 * Copyright 2013, Tan Chee Eng (@tan-ce)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define _XOPEN_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include "helpers.h"

// Caught a signal which indicates we should quit
static volatile int quit_requested = 0;
// Caught sigwinch. Start at 1 so we send the SIGWINCH at the beginning
static volatile int sigwinch_received = 1;

// Handles polling for data to read. The data is then 
// promptly written to stdout
static int poll_pts(struct pollfd *pfd) {
    unsigned char buf[256];
    ssize_t blksz;

    if (pfd->revents & (POLLERR | POLLHUP | POLLNVAL)) {
        return 1;
    }

    if (!(pfd->revents & POLLIN)) return 0;
    
    // Read the data
    blksz = read(pfd->fd, buf, 256);
    if (blksz == -1) {
        perror("Error reading from PTS device");
        return -1;
    }
    
    // EOF
    if (blksz == 0) return 1;

    // Write the data
    return write_to_fd(STDOUT_FILENO, buf, blksz);
}

// Polls stdin for data, and prompty writes it to the tty
static int poll_stdin(struct pollfd *pfd, int pts_fd) {
    unsigned char buf[256];
    ssize_t blksz;

    if (pfd->revents & (POLLERR | POLLHUP | POLLNVAL)) {
        return 1;
    }

    if (!(pfd->revents & POLLIN)) return 0;

    // Read the data
    blksz = read(pfd->fd, buf, 256);
    if (blksz == -1) {
        perror("Error reading from stdin");
        return -1;
    }

    // EOF
    if (blksz == 0) return 1;

    return write_to_fd(pts_fd, buf, blksz);
}

// Handle a signal which should result in termination
static void handle_quit_signals(int sig) {
    quit_requested = 1;
}

// Handle a sigwinch
static void handle_sigwinch(int sig) {
    sigwinch_received = 1;
}

// Reads the current window size and
// passes it along to the PTS device
static void update_winsize(int src_fd, int dst_fd) {
    struct winsize w;

    // Read
    if (ioctl(src_fd, TIOCGWINSZ, &w) == -1) {
        perror("update_winsize: Error getting window size!\n");
        return;
    }

    // Write
    if (ioctl(dst_fd, TIOCSWINSZ, &w) == -1) {
        perror("update_winsize: Error setting window size!\n");
        return;
    }
}

// Installs the relevant signal handlers
// Returns -1 on failure, 0 on success
static int init_signals(void) {
    // Null terminated list of signals which result in termination
    int quit_signals[] = { SIGALRM, SIGHUP, SIGPIPE, SIGQUIT, SIGTERM, SIGINT, 0 };

    struct sigaction act;
    int i;

    memset(&act, '\0', sizeof(act));
    act.sa_flags = SA_RESTART;

    // Install the termination handlers
    act.sa_handler = &handle_quit_signals;
    for (i = 0; quit_signals[i]; i++) {
        if (sigaction(quit_signals[i], &act, NULL) < 0) {
            perror("Error installing signal handler");
            return -1;
        }
    }

    // Catch SIGWINCH
    act.sa_handler = &handle_sigwinch;
    if (sigaction(SIGWINCH, &act, NULL) < 0) {
        perror("Error installing signal handler");
        return -1;
    }

    return 0;
}

// Set-up the terminal properly
// Returns -1 on failure, 0 on success
static int init_terminal(void) {
    struct termios new_tty;

    // Save the current stdin termios
    if (tcgetattr(STDIN_FILENO, &original_tty) < 0) {
        perror("Failed to get current TTY settings");
        return -1;
    }

    // Start from the current settings
    new_tty = original_tty;

    // Make the terminal like an SSH or telnet client
    new_tty.c_iflag |= IGNPAR;
    new_tty.c_iflag &= ~(ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXANY | IXOFF);
    new_tty.c_lflag &= ~(ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHONL);
    new_tty.c_oflag &= ~OPOST;
    new_tty.c_cc[VMIN] = 1;
    new_tty.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_tty) < 0) {
        perror("Failed to set TTY");
        return -1;
    }

    // Get the new settings
    tcgetattr(STDIN_FILENO, &raw_tty);

    return 0;
}

// Restores the terminal to its original state
static void deinit_terminal(void) {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_tty) < 0) {
        perror("Failed to reset TTY");
        return;
    }
}

// Wraps around a pts device like an SSH client
int pts_wrap(int pts_fd) {
    struct pollfd fds[2];

    // PTS file descriptor
    fds[0].fd = pts_fd;
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    // Stdin file descriptor
    fds[1].fd = STDIN_FILENO;
    fds[1].events = POLLIN;
    fds[1].revents = 0;

    // Install signal handlers
    if (init_signals() != 0) return -1;

    // Set up the terminal
    init_terminal();

    // I/O loop to shove data
    while (!quit_requested) {
        int ret;

        // Half a second timeout so we get a chance to respond to 
        // SIGWINCH if nothing new is printed or the user doesn't 
        // press anything on the keyboard
        poll(fds, 2, 500);

        ret = poll_pts(&fds[0]);
        if (ret == 1 || ret != 0) break;

        ret = poll_stdin(&fds[1], pts_fd);
        if (ret == 1 || ret != 0) break;

        if (sigwinch_received) {
            sigwinch_received = 0;
            update_winsize(STDOUT_FILENO, pts_fd);
        }
    }

    // Reset terminal
    deinit_terminal();

    return 0;
}

// Main application entry point
int pts_wrap_main(int argc, char *argv[]) {
    char pts_name[256];
    int pts_fd, ret;

    // Open the PTS device
    pts_fd = pts_open(pts_name, 256);
    if (pts_fd < 0) {
        perror("Error opening PTS device");
        return -1;
    }

    printf("PTS device is %s\n", pts_name);

    ret = pts_wrap(pts_fd);

    // Bye bye...
    printf("\npts-wrap exited\n");
    return ret;
}
