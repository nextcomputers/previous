//
//  i860cfg.h
//  Previous
//
//  Created by Simon Schubiger on 14/01/16.
//
//

#ifndef i860cfg_h
#define i860cfg_h

#define CONF_I860_SPEED     1
#define CONF_I860_DEV       2
#define CONF_I860_NO_THREAD 3

#define CONF_I860 CONF_I860_DEV

#if CONF_I860==CONF_I860_DEV
#define TRACE_I860           0
#define TRACE_RDWR_MEM       0
#define TRACE_PAGE_FAULT     0
#define TRACE_UNDEFINED_I860 1
#define TRACE_UNALIGNED_MEM  1
#define TRACE_EXT_INT        0
#define ENABLE_I860_THREAD   1
#define ENABLE_I860_TLB      1

#elif CONF_I860==CONF_I860_SPEED
#define TRACE_I860           0
#define TRACE_RDWR_MEM       0
#define TRACE_PAGE_FAULT     0
#define TRACE_UNDEFINED_I860 0
#define TRACE_UNALIGNED_MEM  0
#define TRACE_EXT_INT        0
#define ENABLE_I860_THREAD   1
#define ENABLE_I860_TLB      1

#elif CONF_I860==CONF_I860_NO_THREAD
#define TRACE_I860           0
#define TRACE_RDWR_MEM       0
#define TRACE_PAGE_FAULT     0
#define TRACE_UNDEFINED_I860 0
#define TRACE_UNALIGNED_MEM  0
#define TRACE_EXT_INT        0
#define ENABLE_I860_THREAD   0
#define ENABLE_I860_TLB      1

#endif

#endif /* i860cfg_h */
