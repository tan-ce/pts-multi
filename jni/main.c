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

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>

int pts_wrap_main(int argc, char *argv[]);
int pts_shell_main(int argc, char *argv[]);
int pts_exec_main(int argc, char *argv[]);
int pts_daemon_main(int argc, char *argv[]);
int pts_passwd_main(int argc, char *argv[]);

int main(int argc, char *argv[]) {
    int arg_multicall = 0;
    char *callname;

    parse_callname:
    callname = basename(argv[0]);

    if (strcmp(callname, "pts-daemon") == 0) {
        return pts_daemon_main(argc, argv);
    } else if (strcmp(callname, "pts-shell") == 0) {
        return pts_shell_main(argc, argv);
    } else if (strcmp(callname, "pts-exec") == 0) {
        return pts_exec_main(argc, argv);
    } else if (strcmp(callname, "pts-wrap") == 0) {
        return pts_wrap_main(argc, argv);
    } else if (strcmp(callname, "pts-passwd") == 0) {
        return pts_passwd_main(argc, argv);
    } else {
        if (argc < 2 || arg_multicall) {
            printf("Info: Multicall binary for:\n"
                   "* pts-daemon\n"
                   "* pts-shell\n"
                   "* pts-passwd\n"
                   "* pts-exec\n"
                   "* pts-wrap\n");
            return -1;
        }

        argv = &argv[1];
        argc--;
        arg_multicall = 1;
        goto parse_callname;
    }

    return -1;
}
