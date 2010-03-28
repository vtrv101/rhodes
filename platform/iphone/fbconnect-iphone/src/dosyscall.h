/*
 *  dosyscall.h
 *  FBConnect
 *
 *  Created by Vlad on 3/27/10.
 *  Copyright 2010 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef __RHO_DO_SYSCALL_H__
#define __RHO_DO_SYSCALL_H__

#if defined(__cplusplus)
extern "C" {
#endif

	PARAMS_WRAPPER* fb_do_syscall(PARAMS_WRAPPER* params);
	
#if defined(__cplusplus)
}
#endif

#endif	/* __RHO_DO_SYSCALL_H__ */

