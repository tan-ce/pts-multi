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

#include "helpers.h"
#include "bcrypt.h"

// Returns the new password to set to, or NULL if
// something goes wrong
char *get_new_passwd(void) {
    static char pwd1[128];
    char pwd2[128];
    char *resp;

    printf("Enter new password: ");
    resp = fgets(pwd1, sizeof(pwd1), stdin);
    if (!resp) {
        return NULL;
    } else terminate_buf(pwd1, sizeof(pwd1));

    printf("Enter new password (again): ");
    resp = fgets(pwd2, sizeof(pwd2), stdin);
    if (!resp) {
        memset(pwd1, '\0', sizeof(pwd1));
        return NULL;
    } else terminate_buf(pwd2, sizeof(pwd2));

    if ((strlen(pwd1) != strlen(pwd2))
        || (strcmp(pwd1, pwd2) != 0)) {
        printf("Passwords do not match\n");
        memset(pwd1, '\0', sizeof(pwd1));
        memset(pwd2, '\0', sizeof(pwd2));
        return NULL;
    }

    memset(pwd2, '\0', sizeof(pwd2));
    return pwd1;
}

// Save the new password
int set_passwd(FILE *passwd, char *newpwd) {
    char *hash;
    size_t len;

    // Calculate the hash
    hash = bcrypt(newpwd, bcrypt_gensalt(11));

    // Rewind the stream
    if (fseek(passwd, 0, SEEK_SET) < 0) {
        perror("Unable to rewind passwd file");
        return -1;
    }

    // Write 'em
    len = strlen(hash) + 1; // Include NUL
    if (fwrite(hash, 1, len, passwd) != len) {
        printf("Warning: An error might have occured writing the password\n");
    }

    return 0;
}

int pts_passwd_main(int argc, char *argv[]) {
    char hash[61];
    FILE *passwd = 0;
    size_t len;
    int ret = 0;
    char *newpwd;
    
    // Check our configuration directory
    if (check_path(PATH_PREFIX)) return -1;

    // Get a handle on the passwd file
    len = 60;
    passwd = get_file(PATH_PREFIX "/passwd", hash, &len);
    hash[len] = '\0';

    // Make the terminal suitable for password entry
    passwd_init_terminal();

    // Ask for the current password
    if (len) {
        ret = verify_password("Enter CURRENT password: ", hash);
        if (ret) {
            printf("Authentication failed\n");
            goto cleanup;
        }
    } else {
        printf("Setting password for the first time\n");
    }

    // Get the new password
    newpwd = get_new_passwd();
    if (!newpwd) {
        printf("Password unchanged\n");
        ret = 1;
        goto cleanup;
    }

    ret = set_passwd(passwd, newpwd);
    if (ret) {
        printf("Password unchanged\n");
    } else {
        printf("Password updated\n");
    }

cleanup:
    if (passwd) fclose(passwd);
    passwd_deinit_terminal();
    return ret;
}
