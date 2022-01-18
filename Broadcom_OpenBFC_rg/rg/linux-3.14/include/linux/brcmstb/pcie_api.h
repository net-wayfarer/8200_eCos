/*
 * Copyright © 2015-2016 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation (the "GPL").
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * A copy of the GPL is available at
 * http://www.broadcom.com/licenses/GPLv2.php or from the Free Software
 * Foundation at https://www.gnu.org/licenses/ .
 */

#ifndef _BRCMSTB_PCIE_API_H
#define _BRCMSTB_PCIE_API_H

#include <linux/types.h>
#include <linux/pci.h>

struct regulator_cntrl_info {
	bool regulator_state;
	char name[32];
};

int brcm_pcie_max_regulator_supported(void);
int brcm_pcie_regulator_info(struct pci_dev *pdev,
				struct regulator_cntrl_info *reg_cntrl_info);
#endif /* _BRCMSTB_PCIE_API_H */