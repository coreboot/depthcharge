/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _ANDROID_KERNEL_CMDLINE_H_
#define _ANDROID_KERNEL_CMDLINE_H_

#include <vb2_api.h>

void android_kernel_cmdline_fixup(struct vb2_kernel_params *kparams);

#endif /* _ANDROID_KERNEL_CMDLINE_H_ */
