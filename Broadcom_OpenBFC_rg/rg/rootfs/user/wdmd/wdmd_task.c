/*
 * Copyright 2016 Broadcom Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <inttypes.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <ctype.h>
#include <syslog.h>

#include "wdmd.h"
#include "wdmd_task.h"

struct proc_list {
    struct proc_list *next;
    char *name;
    char *path;
    int pid;
    int strikes;
};
#define YOURE_OUT 3 /* 3 strikes and you're out */

/* number of seconds to wait before monitoring to allow apps to start */
#define MONITOR_DELAY 180

static uint64_t start_time;

static struct proc_list *plist;
static int proc_cnt = 0;
static char *task_file = NULL;
static int reprocess_file = 0;
static int ignore_tasks = 0;

static int isfname(char ch)
{
    return (isalpha(ch) ||
            isdigit(ch) ||
            ch == '.'   ||
            ch == '/'   ||
            ch == '\\'  ||
            ch == '-'   ||
            ch == '_'   ||
            ch == ':'   ||
            ch == '@'   ||
            ch == '#');
}

static int match_name(const char *file, const char *name)
{
    char name_buf[256];
    int bytes, num;

    int fd = open(file, O_RDONLY);
    if (fd < 0)
        return 0;

    num = 0;
    name_buf[0] = 0;
    while ((bytes = read(fd, &name_buf[num], sizeof(name_buf) - num)) > 0)
        num += bytes;

    close(fd);

    int i = 0;

    /* null terminate the string */
    while (isfname(name_buf[i]))
        i++;
    if (i < sizeof(name_buf))
        name_buf[i] = 0;

    return strcmp(name, name_buf) == 0;
}

static int is_pid(const char *name)
{
    int pid = 0;
    while (*name) {
        if (*name < '0' || *name > '9')
            return 0;
        pid *= 10;
        pid += *name - '0';
        name++;
    }
    return pid;
}

static int find_pid(struct proc_list *proc)
{
    struct dirent *dent;
    DIR *dir = opendir("/proc");
    char name_buf[256];
    if (!dir)
        return 0;

    while ((dent = readdir(dir))) {
        int pid = is_pid(dent->d_name);
        if (pid) {
            strcpy(name_buf, "/proc/");
            strcat(name_buf, dent->d_name);
            strcat(name_buf, "/comm");
            if (match_name(name_buf, proc->name)) {
                int len = sizeof("/proc/") + strlen(dent->d_name);
                proc->path = malloc(len);
                strcpy(proc->path, "/proc/");
                strcat(proc->path, dent->d_name);
                proc->pid = pid;
                proc->strikes = 0;
                closedir(dir);
                return pid;
            }
        }
    }
    closedir(dir);
    return 0;
}

static void free_proc_list(void)
{
    struct proc_list *proc = plist;
    while (proc) {
        struct proc_list *next = proc->next;
        if (proc->path)
                free(proc->path);
        if (proc->name)
                free(proc->name);
        free(proc);
        proc = next;
    }
    plist = NULL;
    proc_cnt = 0;
}

int wdmd_process_file(char *file)
{
    typedef enum {
        eol_comment_reading,
        c_comment_reading,
        name_reading,
        no_idea_what_im_doing
    }   read_state;

    FILE *fp = fopen(file, "r");
    if (!fp)
        return -1;

    syslog(LOG_WARNING, "reading task file %s\n", file);
    printf("reading task file %s\n", file);

    task_file = file;

    start_time = monotime();

    read_state rstate = no_idea_what_im_doing;
    struct proc_list *proc = NULL;

    char name[256];
    int ch;
    int i = 0;

    while ((ch = fgetc(fp)) > 0) {
        if (rstate == no_idea_what_im_doing) {
            if (isspace(ch))
                continue;
            if (ch == '#') {
                rstate = eol_comment_reading;
                continue;
            }
            if (ch == '/') {
                ch = fgetc(fp);
                if (ch == '*')
                    rstate = c_comment_reading;
                else
                    rstate = eol_comment_reading;
                continue;
            }
            if (isfname(ch)) {
                rstate = name_reading;
                proc_cnt++;
                if (proc) {
                    proc->next = malloc(sizeof(*proc));
                    proc = proc->next;
                }
                else {
                    proc = plist = malloc(sizeof(*proc));
                }
                proc->next = NULL;
                proc->path = NULL;
                proc->name = NULL;
                proc->pid = 0;
                proc->strikes = 0;
                i = 0;
                name[i++] = ch;
            }
            continue;
        }
        if (rstate == name_reading) {
            if (isfname(ch)) {
                name[i++] = ch;
            }
            else {
                name[i++] = 0;
                proc->name = malloc(i);
                strcpy(proc->name, name);
                rstate = no_idea_what_im_doing;
                i = 0;
                syslog(LOG_WARNING, "wdmd monitoring task %s\n", proc->name);
                printf("wdmd monitoring task %s\n", proc->name);
            }
            continue;
        }
        if (rstate == eol_comment_reading) {
            if (ch == '\n')
                rstate = no_idea_what_im_doing;
            continue;
        }
        if (rstate == c_comment_reading) {
            if (ch == '*') {
                ch = fgetc(fp);
                if (ch == '/') {
                    rstate = no_idea_what_im_doing;
                }
            }
            continue;
        }
    }
    if (rstate == name_reading) {
        name[i++] = 0;
        proc->name = malloc(i);
        strcpy(proc->name, name);
    }
    fclose(fp);

    return proc_cnt;
}

int wdmd_test_tasks(void)
{
    struct proc_list *proc = plist;
    int fail_count = 0;
    struct stat st;

    /* give apps some time to start */
    if ((monotime() - start_time) < MONITOR_DELAY)
        return 0;

    if (ignore_tasks)
        return 0;

    if (reprocess_file && task_file) {
        free_proc_list();
        wdmd_process_file(task_file);
        proc = plist;
        reprocess_file = 0;
    }

    while (proc) {
        if (!proc->path || stat(proc->path, &st) < 0) {
            if (proc->path) {
                free(proc->path);
                proc->path = NULL;
            }
            int pid = find_pid(proc);
            if (!pid) {
                proc->strikes++;
                if (proc->strikes >= YOURE_OUT)
                    fail_count++;
                syslog(LOG_ERR, "%s died\n", proc->name);
                fprintf(stderr, "%s died\n", proc->name);
            }
        }
        proc = proc->next;
    }
    return fail_count;
}

void wdmd_reprocess_file(void)
{
    reprocess_file = 1;
}

void wdmd_ignore_tasks(int ignore)
{
	ignore_tasks = ignore;
	printf("watchdog is %s tasks", ignore ? "ignoring" : "watching");
}

#if 0

int main(int argc, char *argv[])
{
    if (argc < 2)
        return 0;

    wdmd_process_file(argv[1]);

    printf("\n%d processes:\n", proc_cnt);

    struct proc_list *proc = plist;

    while (proc) {
        int pid = find_pid(proc);
        printf("%10s %7d path %s\n", proc->name,  pid, proc->path);
        proc = proc->next;
    }

    while (1) {
        int failures = wdmd_test_tasks();
        if (failures)
            printf("%d failures\n", failures);
        sleep(1);
    }
    return 0;
}

#endif
