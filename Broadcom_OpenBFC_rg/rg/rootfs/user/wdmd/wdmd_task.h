/*
 * Copyright 2016 Broadcom Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#ifndef __WDMD_TASK_H__
#define __WDMD_TASK_H__

int wdmd_process_file(char *file);
int wdmd_test_tasks(void);
void wdmd_reprocess_file(void);
void wdmd_ignore_tasks(int ignore);

#endif
