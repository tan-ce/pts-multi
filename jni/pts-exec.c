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
#include <paths.h>
#include <termios.h>
#include <sys/ioctl.h>

static void usage() {
    printf(
        "Usage: pts-exec <pts device path> <command> <arg 1> ... <arg n>\n"
    );
}

int pts_exec(char *dev_name, char **cmd_argv) {
    int pts_fd;

    // Disassociate from terminal
    if (setsid() == (pid_t) -1) {
        perror("WARNING, setsid() failed");
    }

    // Open the PTS device
    pts_fd = open(dev_name, O_RDWR);
    if (pts_fd == -1) {
        perror("Failed to open PTS device");
        return EXIT_FAILURE;
    }

    // Replace std{in,out,err}
    dup2(pts_fd, 0);
    dup2(pts_fd, 1);
    dup2(pts_fd, 2);

    // Launch the target command
    execv(cmd_argv[0], cmd_argv);
    perror("Failed to execv()");
    return EXIT_FAILURE;
}

int pts_exec_main(int argc, char *argv[]) {
    char *dev_name;
    char **cmd_argv;
    pid_t pid;

    if (argc < 3) {
        usage();
        return 1;
    }

    dev_name = argv[1];
    cmd_argv = &argv[2];
    
    // Fork
    pid = fork();
    if (pid == -1) {
        perror("Failed to fork");
        return -1;
    }
    if (pid > 0) {
        // In parent
        printf("PID of child = %d\n", pid);
        return 0;
    }
    
    // In child
    return pts_exec(dev_name, cmd_argv);
}
