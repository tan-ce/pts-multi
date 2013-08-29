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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "helpers.h"
#include "bcrypt.h"

int pts_exec(char *dev_name, char **cmd_argv);

// Initialize signal handlers
// Returns 0 on success
int init_signals(void) {
    struct sigaction act;
    memset(&act, '\0', sizeof(act));

    // Ignore SIGPIPE
    act.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &act, NULL);

    // Automatically reap children zombies
    act.sa_handler = SIG_IGN;
    act.sa_flags = SA_NOCLDWAIT;
    sigaction(SIGCHLD, &act, NULL);

    return 0;
}

// Restore signal handler behaviour
void signals_default(void) {
    struct sigaction act;
    memset(&act, '\0', sizeof(act));
    act.sa_handler = SIG_DFL;

    sigaction(SIGPIPE, &act, NULL);
    sigaction(SIGCHLD, &act, NULL);
}

// Creates the control socket
// Returns the socket FD on success, or -1 on failure
int init_socket(const char *sock_path) {
    int sck;
    struct sockaddr_un sck_addr;

    // Create the socket
    sck = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sck == -1) {
        perror("Failed to open socket");
        return -1;
    }

    // Set the path
    sck_addr.sun_family = AF_UNIX;
    strncpy(sck_addr.sun_path, sock_path, sizeof(sck_addr.sun_path));
    sck_addr.sun_path[sizeof(sck_addr.sun_path) - 1] = '\0';

    // Remove if anything is already where the socket should be
    unlink(sck_addr.sun_path);

    printf("Attempting to bind to %s\n", sck_addr.sun_path);

    // Bind
    if (bind(sck, (struct sockaddr *) &sck_addr, sizeof(sck_addr)) < 0) {
        perror("Failed to bind socket");
        return -1;
    }

    // Attempt to set socket permissions
    if (chmod(sck_addr.sun_path, S_IRUSR | S_IRGRP | S_IROTH | 
                    S_IWUSR | S_IWGRP | S_IWOTH) < 0) {
        perror("Warning: unable to set sock permissions");
    }

    return sck;
}

// Handles password authentication
// Returns a "boolean" value indicating whether the password matched
static int service_auth(const char *pwd) {
    char hash_to_match[61];
    char *user_hash;

    if (!pwd) pwd = "";
    
    // Read the password hash from the passwd file
    if (load_file(PATH_PREFIX "/passwd", hash_to_match, 60) < 0) {
        printf("Warning: Unable to read passwd file!\n");
        return 0;
    }
    hash_to_match[60] = '\0';

    // Calculate the hash
    user_hash = bcrypt(pwd, hash_to_match);
    if (user_hash[0] == ':') {
        printf("Warning: passwd file contains an invalid hash\n");
        return 0;
    }

    // Check if it matches
    if ((strlen(hash_to_match) != strlen(user_hash)) ||
        (strcmp(hash_to_match, user_hash) != 0)) {
        return 0;
    }

    return 1;
}

#define EXEC_MAX_ARGS   32
static void service_exec(FILE *fp, char *arg) {
    char *pts, *tmp, *argv[EXEC_MAX_ARGS + 1];
    pid_t pid;
    int i;

    // Parse the TTY device path
    pts = strtok(arg, " ");
    tmp = strtok(NULL, " ");

    // Parse argv
    if (!tmp) {
        fprintf(fp, "0 No file specified\n");
        return;
    }

    argv[0] = tmp;
    for (i = 1; i < EXEC_MAX_ARGS; i++) {
        argv[i] = strtok(NULL, " ");
        if (!argv[i]) {
            i = -1;
            break;
        }
    }

    // Sorry, we don't have enough buffers for argv
    if (i != -1 && (argv[EXEC_MAX_ARGS] = strtok(NULL, " "))) {
        fprintf(fp, "0 Too many arguments in command\n");
        return;
    }

    // Fork
    pid = fork();
    if (pid == -1) {
        fprintf(fp, "0 Failed to fork");
        return;
    }
    if (pid > 0) {
        // In parent
        fprintf(fp, "1 Child launched with PID = %d\n", pid);
        return;
    }
    
    // In child
    signals_default();

    // Exec!
    pts_exec(pts, argv);
    printf("Warning: pts_exec failed\n");
    exit(EXIT_FAILURE);
}

// Handles a single connection. Will fork and close the FD
// in the parent
void service_main(int sck) {
    pid_t pid;
    int authed;
    FILE *fp;
    char buf[128];

    pid = fork();
    if (pid < 0) {
        perror("service_handler(): Could not fork");
        return;
    } else if (pid > 0) {
        // In parent
        close(sck);
        return;
    }

    // In child
    authed = 0;
    pid = getpid();

    // Turn our socket operations into buffered I/O
    fp = fdopen(sck, "w+");
    if (!fp) {
        perror("fdopen failed");
        close(sck);
        exit(EXIT_FAILURE);
    }

    // Service loop
    printf("[%d] Starting service loop\n", pid);
    while(1) {
        char *line = fgets(buf, 128, fp);
        char *cmd, *arg;

        if (!line) break;

        // Parse the command
        cmd = strtok(line, " ");
        arg = strtok(NULL, "\n");

        if (strcmp(cmd, "auth") == 0) {
            authed = service_auth(arg);
            if (authed) {
                fprintf(fp, "1 Auth OK\n");
            } else {
                fprintf(fp, "0 Auth failed\n");
            }

        } else if (strcmp(cmd, "exec") == 0) {
            if (authed) {
                service_exec(fp, arg);
            } else {
                fprintf(fp, "0 Not authorized\n");
            }
        } else {
            fprintf(fp, "0 Bad command\n");
        }
    }

    fclose(fp);
    printf("[%d] Child exited\n", pid);
    exit(EXIT_SUCCESS);
}

// Daemon entry point
int pts_daemon_main(int argc, char *argv[]) {
    int sck;
    struct pollfd pfd;
    
    if (argc > 1 && (strcmp(argv[1], "-D") == 0)) {
        daemonize();
    }

    if (check_path(PATH_PREFIX)) return -1; 

    // Initialization
    printf("Initializing daemon\n");
    if (init_signals()) return -1;

    sck = init_socket(PATH_PREFIX "/pts");
    if (sck < 0) {
        return -1;
    }

    // Main server loop
    pfd.fd = sck;
    pfd.events = POLLIN;

    printf("Entering main loop\n");
    listen(sck, 2);

    while(1) {
        int ret;

        // Poll for incoming connections
        pfd.revents = 0;
        ret = poll(&pfd, 1, -1);
        if (ret < 0) {
            perror("poll() failed in main loop");
            return -1;
        }

        if (pfd.revents & POLLIN) {
            int chd_sck;
            // Incoming connection
            chd_sck = accept(sck, NULL, NULL);
            if (chd_sck < 0) {
                perror("accept() failed in main loop");
                return -1;
            }

            service_main(chd_sck);
        }
    }

    return 0;
}
