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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <linux/limits.h>

#include "helpers.h"
#include "bcrypt.h"

int pts_wrap(int pts_fd);

// Connects to a unix socket. On success a FD is returned.
// Otherwise -1 is returned.
static int unix_socket_connect(const char *path) {
    struct sockaddr_un addr;
    int fd;

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("Could not create socket");
        return -1;
    }

    memset(&addr, '\0', sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        perror("Could not connect to socket");
        close(fd);
        return -1;
    }

    return fd;
}

// Server responses are only 1 (success), 0 (failure)
// A parse failure will return -1
static int parse_server_response(FILE *fp, char **msg) {
    static char buf[256];
    int ret;

    // Expected "1" or "0"
    ret = fgetc(fp);
    if (ret == EOF) {
        return -1;
    }

    switch ((char) ret) {
        case '0': ret = 0; break;
        case '1': ret = 1; break;
        default: 
            return -1;
    }

    // Discard the space
    if (fgetc(fp) == EOF) {
        return -1;
    }

    // Get the rest of the message
    if (!fgets(buf, 256, fp)) {
        return -1;
    }

    terminate_buf(buf, sizeof(buf));

    *msg = buf;
    return ret;
}

// Authenticate with the daemon
static void authenticate(FILE *fp, char *pwd) {
    char *tmp;
    int ret;

    // Authenticate with the daemon
    if (fprintf(fp, "auth %s\n", pwd) < 0) {
        fprintf(stderr, "Unable to communicate with daemon\n");
        exit(-1);
    }

    ret = parse_server_response(fp, &tmp);
    if (ret == -1) {
        fprintf(stderr, "Server returned unexpected response\n");
        exit(-1);
    } else if (ret == 0) {
        fprintf(stderr, "Authentication failed: %s\n", tmp);
        exit(-1);
    }

}

static void request_exec(FILE *fp, char *pts_name, char *argv[]) {
    int i, ret;
    char *tmp;

    if (fprintf(fp, "exec %s ", pts_name) < 0) {
        fprintf(stderr, "Unable to communicate with daemon\n");
        exit(-1);
    }

    i = 0;
    while(1) {
        if (argv[i + 1]) {
            fprintf(fp, "%s ", argv[i]);
        } else {
            fprintf(fp, "%s\n", argv[i]);
            break;
        }
    }

    ret = parse_server_response(fp, &tmp);
    if (ret == -1) {
        fprintf(stderr, "Server returned unexpected response\n");
        exit(-1);
    } else if (ret == 0) {
        fprintf(stderr, "Launch failed: %s\n", tmp);
        exit(-1);
    }
}

int pts_shell_main(int argc, char *argv[]) {
    char buf[256], *buf2;
    int sck, i, pts_fd;
    FILE *fp;

    // Check the arguments
    if (argc <= 1) {
        printf("No command specified?\n");
        return 1;
    }

    // Show the user the command
    printf("(pts-shell) ");
    for (i = 1; i < argc; i++) {
        printf("%s ", argv[i]);
    }
    printf("\n");

    // Connect!
    sck = unix_socket_connect("/dev/pts-daemon");
    if (sck == -1) return -1;

    fp = fdopen(sck, "w+");
    if (!fp) {
        perror("fdopen");
        close(sck);
        return -1;
    }

    // See if the password is specified on the command line
    buf2 = getenv("PTS_AUTH");
    if (buf2) {
        // User supplied password in the environment
        authenticate(fp, buf2);
    } else {
        // Get the user's password
        passwd_init_terminal();
        printf("(pts-shell) Enter your password: ");
        if (fgets(buf, sizeof(buf), stdin) == NULL) return -1;
        passwd_deinit_terminal();
        terminate_buf(buf, sizeof(buf));

        authenticate(fp, buf);
        memset(buf, '\0', sizeof(buf));
    }

    // Send the daemon our current directory
    buf2 = malloc(PATH_MAX);
    if (getcwd(buf2, PATH_MAX)) {
        fprintf(fp, "cd %s\n", buf2);
        free(buf2);

        i = parse_server_response(fp, &buf2);
        if (i == -1) {
            fprintf(stderr, "Server returned unexpected response\n");
            exit(-1);
        } else if (i == 0) {
            fprintf(stderr, "Warning: Unable to change directory: %s\n", buf2);
        }
    } else {
        free(buf2);
        fprintf(stderr, "Warning: Could not get current working directory\n");
    }

    // Open a new PTS device
    pts_fd = pts_open(buf, 256);
    if (pts_fd < 0) {
        perror("Error opening PTS device");
        return -1;
    }

    // Invoke the app
    request_exec(fp, buf, &argv[1]);
    fclose(fp);

    // And call pts-wrap
    i = pts_wrap(pts_fd);
    printf("\npts-shell exited\n");
    return i;
}
