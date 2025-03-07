/*
 * Macros to instrument PM reads and writes
 * Author : Nalli, S. 
 * Contact: nalli@wisc.edu
 *
 * The value returned by code surrounded by {}
 * is the value returned by last statement in
 * the block. These macros do not perform any
 * operations on the persistent variable itself,
 * and hence do not introduce any extra accesses
 * to persistent memory. 
 * 
 */

#ifndef PM_INSTR_H
#define PM_INSTR_H
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <string.h>

#define LOC1 	__func__	/* str ["0"]: can be __func__, __FILE__ */
#define LOC2	__LINE__        /* int [0]  : can be __LINE__ */

#define m_out stdout
#define m_err stderr

#define TSTR_SZ         128
#define MAX_TBUF_SZ     512*1024*1024   /* bytes */

extern __thread struct timeval mtm_time;
extern __thread int mtm_tid;

extern __thread char tstr[TSTR_SZ];
extern __thread unsigned long long tsz;
extern __thread unsigned long long tbuf_ptr;

extern char *tbuf;
extern unsigned long long tbuf_sz;
extern pthread_spinlock_t tbuf_lock, tot_epoch_lock;
extern int mtm_enable_trace;
extern int mtm_debug_buffer;
extern int tracing_on, trace_marker;
extern struct timeval glb_time;
extern unsigned long long start_buf_drain, end_buf_drain, buf_drain_period;
extern unsigned long long glb_tv_sec, glb_tv_usec, glb_start_time;
extern unsigned long long tot_epoch;

extern __thread int reg_write;
extern __thread unsigned long long n_epoch;
extern void __pm_trace_print(int,  ...);
extern unsigned long long get_tot_epoch_count(void);


#ifdef _ENABLE_TRACE
/* Custom user-mode, blocking tracer */
#define time_since_start							\
({					                                \
  gettimeofday(&mtm_time, NULL);					\
  (((1000000*mtm_time.tv_sec + 				     	\
    mtm_time.tv_usec)				                \
  - (glb_start_time)) - buf_drain_period);			\
})

#define TENTRY_ID								\
(mtm_tid == -1 ? 							\
  ({mtm_tid = syscall(SYS_gettid); mtm_tid;}) : mtm_tid), 	\
(time_since_start)

#define pm_trace_print(format, args ...)					\
{										\
	if(mtm_enable_trace) {							\
       pthread_spin_lock(&tbuf_lock);						\
       sprintf(tstr, format, args);    					\
       tsz = strlen(tstr);							\
       if(tsz < MAX_TBUF_SZ - tbuf_sz)						\
           {									\
              memcpy(tbuf+tbuf_sz, 						\
                tstr, tsz);					\
              tbuf_sz += tsz;							\
          }									\
          else {									\
              tbuf_ptr = 0;							\
              if(mtm_debug_buffer)						\
                  {								\
			/* fprintf(m_err,*/ 					\
			/* "start_buf_drain - %llu us\n", */			\
                     ((start_buf_drain = time_since_start));			\
                 }								\
                 while(tbuf_ptr < tbuf_sz)					\
                  {								\
                     tbuf_ptr = tbuf_ptr + 1 + 				\
                     fprintf(m_out,"%s", 				\
                        tbuf + tbuf_ptr);				\
                 }								\
                 tbuf_sz = 0; 							\
                 memset(tbuf,'\0', MAX_TBUF_SZ);					\
                 if(mtm_debug_buffer)						\
                  {								\
			/* fprintf(m_err, */ 					\
			/* "end_buf_drain - %llu us\n",	*/			\
                     ((end_buf_drain = time_since_start));			\
                 }								\
                 memcpy(tbuf, tstr, tsz);					\
                 tbuf_sz += tsz;							\
                 buf_drain_period += (end_buf_drain -				\
                   start_buf_drain) + 1;			\
             }									\
             pthread_spin_unlock(&tbuf_lock);					\
         }									\
     }
#elif _ENABLE_FTRACE
/* Standard kernel-mode, non-blocking tracer.  */
#define TENTRY_ID (int)0 ,(unsigned long long)0
#define pm_trace_print(format, args ...)					\
     {										\
       if(mtm_enable_trace) {							\
           sprintf(tstr, format, args);    				\
           tsz = strlen(tstr);						\
           assert (0 < write(trace_marker, tstr+4, tsz-4));		\
       }									\
   }
#else
/* No tracing */
#define TENTRY_ID (int)0
#define pm_trace_print(format, args ...)					\
   {										\
    __pm_trace_print(0, args);                                         	\
}
#endif

#define PM_TRACE                        pm_trace_print

/* Cacheable PM write */
#define PM_WRT_MARKER                   "PM_W"
#define PM_DWRT_MARKER                  "PM_DW"
#define PM_DI_MARKER                    "PM_DI"

/* Cacheable PM read */
#define PM_RD_MARKER                    "PM_R"

/* Un-cacheable PM store */
#define PM_NTI                          "PM_I"

/* PM flush */
#define PM_FLUSH_MARKER                 "PM_L"
#define PM_FLUSHOPT_MARKER              "PM_O"

/* PM Delimiters */
#define PM_TX_START                     "PM_XS"
#define PM_FENCE_MARKER                 "PM_N"
#define PM_COMMIT_MARKER                "PM_C"
#define PM_BARRIER_MARKER               "PM_B"
#define PM_TX_END                       "PM_XE"

/* PM Write macros */
/* PM Write to variable */
#define PM_STORE(pm_dst, bytes)                     	\
({                                              	\
    PM_TRACE("%d:%llu:%s:%p:%lu:%s:%d\n",        	\
     TENTRY_ID,		    	\
     PM_WRT_MARKER,              	\
     (pm_dst),                   	\
     bytes,			    	\
     LOC1,                   	\
     LOC2);                  	\
})

#define PM_WRITE(pm_dst)                            	\
({                                              	\
    PM_TRACE("%d:%llu:%s:%p:%lu:%s:%d\n",        	\
     TENTRY_ID,		    	\
     PM_WRT_MARKER,              	\
     &(pm_dst),                  	\
     sizeof((pm_dst)),           	\
     LOC1,                   	\
     LOC2);                  	\
    \
    pm_dst;                                     	\
})

#define PM_EQU(pm_dst, y)                           	\
({                                              	\
    PM_TRACE("%d:%llu:%s:%p:%lu:%s:%d\n",    	\
     TENTRY_ID,		    	\
     PM_WRT_MARKER,              	\
     &(pm_dst),                  	\
     sizeof((pm_dst)),           	\
     LOC1,                   	\
     LOC2);                  	\
    pm_dst = y;                             	\
})

#define PM_OR_EQU(pm_dst, y)                        	\
({                                              	\
    PM_TRACE("%d:%llu:%s:%p:%lu:%s:%d\n",    	\
     TENTRY_ID,		    	\
     PM_WRT_MARKER,              	\
     &(pm_dst),                  	\
     sizeof((pm_dst)),           	\
     LOC1,                   	\
     LOC2);                  	\
    pm_dst |= y;                            	\
})

#define PM_AND_EQU(pm_dst, y)                       	\
({                                              	\
    PM_TRACE("%d:%llu:%s:%p:%lu:%s:%d\n",    	\
     TENTRY_ID,		    	\
     PM_WRT_MARKER,              	\
     &(pm_dst),                  	\
     sizeof((pm_dst)),           	\
     LOC1,                   	\
     LOC2);                  	\
    pm_dst &= y;                            	\
})

#define PM_ADD_EQU(pm_dst, y)                       	\
({                                              	\
    PM_TRACE("%d:%llu:%s:%p:%lu:%s:%d\n",    	\
     TENTRY_ID,		    	\
     PM_WRT_MARKER,              	\
     &(pm_dst),                  	\
     sizeof((pm_dst)),           	\
     LOC1,                   	\
     LOC2);                  	\
    pm_dst += y;                            	\
})

#define PM_SUB_EQU(pm_dst, y)                       	\
({                                              	\
    PM_TRACE("%d:%llu:%s:%p:%lu:%s:%d\n",    	\
     TENTRY_ID,		    	\
     PM_WRT_MARKER,              	\
     &(pm_dst),                  	\
     sizeof((pm_dst)),           	\
     LOC1,                   	\
     LOC2);                  	\
    pm_dst -= y;                            	\
})

/* PM Writes to a range of memory */
#define PM_MEMSET(pm_dst, val, sz)                  	\
({                                              	\
    PM_TRACE("%d:%llu:%s:%p:%lu:%s:%d\n",    	\
     TENTRY_ID,		    	\
     PM_WRT_MARKER,              	\
     (pm_dst),                   	\
     (unsigned long)sz,          	\
     LOC1,                   	\
     LOC2);                  	\
    memset(pm_dst, val, sz);                	\
}) 

#define PM_MEMCPY(pm_dst, src, sz)                  	\
({                                              	\
    PM_TRACE("%d:%llu:%s:%p:%lu:%s:%d\n",    	\
     TENTRY_ID,		    	\
     PM_WRT_MARKER,              	\
     (pm_dst),                   	\
     (unsigned long)sz,          	\
     LOC1,                   	\
     LOC2);                  	\
    memcpy(pm_dst, src, sz);                	\
})              

#define PM_DMEMCPY(pm_dst, src, sz)                  	\
({                                              	\
    PM_TRACE("%d:%llu:%s:%p:%lu:%s:%d\n",    	\
     TENTRY_ID,		    	\
     PM_DWRT_MARKER,              	\
     (pm_dst),                   	\
     (unsigned long)sz,          	\
     LOC1,                   	\
     LOC2);                  	\
    memcpy(pm_dst, src, sz);                	\
})              

#define PM_RNGCPY(pm_dst, sz)                  		\
({                                              	\
    PM_TRACE("%d:%llu:%s:%p:%lu:%s:%d\n",    	\
     TENTRY_ID,		    	\
     PM_WRT_MARKER,              	\
     (pm_dst),                   	\
     (unsigned long)sz,          	\
     LOC1,                   	\
     LOC2);                  	\
})              

#define PM_STRCPY(pm_dst, src, bytes)                   \
({                                              	\
    PM_TRACE("%d:%llu:%s:%p:%u:%s:%d\n",     	\
     TENTRY_ID,		    	\
     PM_WRT_MARKER,              	\
     (pm_dst),                   	\
     (int)bytes,    	    		\
     LOC1,                   	\
     LOC2);                  	\
    strcpy(pm_dst, src);                    	\
})

#define PM_MOVNTI(pm_dst, count, copied)            	\
({                                              	\
    PM_TRACE("%d:%llu:%s:%p:%lu:%lu:%s:%d\n",	\
     TENTRY_ID,		    	\
     PM_NTI,                     	\
     (pm_dst),                   	\
     (unsigned long)copied,      	\
     (unsigned long)count,       	\
     LOC1,                   	\
     LOC2                    	\
     );                              	\
    0;                                      	\
})

/* PM Read macros */
/* Return the data    of persistent variable */
#define PM_READ(pm_src)                             	\
({                                              	\
    PM_TRACE("%d:%llu:%s:%p:%lu:%s:%d\n",    	\
     TENTRY_ID,		    	\
     PM_RD_MARKER,               	\
     &(pm_src),                  	\
     sizeof((pm_src)),           	\
     LOC1,                   	\
     LOC2);                  	\
    (pm_src);                               	\
})     

/* Return the address of persistent variable */
#define PM_READ_P(pm_src)                           	\
({                                              	\
    PM_TRACE("%d:%llu:%s:%p:%lu:%s:%d\n",    	\
     TENTRY_ID,		    	\
     PM_RD_MARKER,               	\
     &(pm_src),                  	\
     sizeof((pm_src)),           	\
     LOC1,                   	\
     LOC2);                  	\
    &(pm_src);                              	\
})    

/* Return the address of persistent variable */
#define PM_RD_WR_P(pm_src)                          	\
({                                              	\
    PM_TRACE("%d:%llu:%s:%p:%lu:%s:%d\n",        	\
     TENTRY_ID,		    	\
     PM_RD_MARKER,               	\
     &(pm_src),                  	\
     sizeof((pm_src)),           	\
     LOC1,                   	\
     LOC2);                  	\
    PM_TRACE("%d:%llu:%s:%p:%lu:%s:%d\n",        	\
     TENTRY_ID,		    	\
     PM_WRT_MARKER,              	\
     &(pm_src),                  	\
     sizeof((pm_src)),           	\
     LOC1,                   	\
     LOC2);                  	\
    &(pm_src);                                  	\
})    

/* PM Reads to a range of memory */
#define PM_MEMCMP(pm_dst, src, sz)                  	\
({                                              	\
    PM_TRACE("%d:%llu:%s:%p:%lu:%s:%d\n",    	\
     TENTRY_ID,		    	\
     PM_RD_MARKER,               	\
     (pm_dst),                   	\
     (unsigned long)sz,          	\
     LOC1,                   	\
     LOC2);                  	\
    memcmp(pm_dst, src, sz);                	\
})


#define start_epoch()       ({;})
#define end_epoch()         ({;})

#define start_txn()                                 	\
({                                              	\
    PM_TRACE("%d:%llu:%s:%s:%d\n",               	\
     TENTRY_ID,		    	\
     PM_TX_START,                        	\
     LOC1,                           	\
     LOC2);                          	\
})

#define end_txn()                                   	\
({                                              	\
    PM_TRACE("%d:%llu:%s:%s:%d\n",               	\
     TENTRY_ID,		    	\
     PM_TX_END,                          	\
     LOC1,                           	\
     LOC2);                          	\
})

#define PM_START_TX	start_txn
#define PM_END_TX	end_txn

/* PM Persist operations 
 * (done/copied) followed by count to maintain 
 * uniformity with other macros
 */
#define PM_FLUSHOPT(pm_dst, count, done)               	\
({                                              	\
    PM_TRACE("%d:%llu:%s:%p:%u:%u:%s:%d\n",      	\
     TENTRY_ID,		    	\
     PM_FLUSHOPT_MARKER,                	\
     (pm_dst),                       	\
     done,                           	\
     count,                          	\
     LOC1,                       	\
     LOC2                        	\
     );                                  	\
})

#define PM_FLUSH(pm_dst, count, done)               	\
({                                              	\
    PM_TRACE("%d:%llu:%s:%p:%u:%u:%s:%d\n",      	\
     TENTRY_ID,		    	\
     PM_FLUSH_MARKER,                	\
     (pm_dst),                       	\
     done,                           	\
     count,                          	\
     LOC1,                       	\
     LOC2                        	\
     );                                  	\
})
#define PM_COMMIT()                                 	\
({                                              	\
    PM_TRACE("%d:%llu:%s:%s:%d\n",		    	\
     TENTRY_ID,		    	\
     PM_COMMIT_MARKER,    	    	\
     LOC1, 				\
     LOC2);            		\
})
#define PM_BARRIER()                                	\
({                                              	\
    PM_TRACE("%d:%llu:%s:%s:%d\n",		    	\
     TENTRY_ID,		    	\
     PM_BARRIER_MARKER,   	    	\
     LOC1, 				\
     LOC2);            		\
})
#define PM_FENCE()                                  	\
({                                              	\
    PM_TRACE("%d:%llu:%s:%s:%d\n",		    	\
     TENTRY_ID,		    	\
     PM_FENCE_MARKER,     	    	\
     LOC1, 				\
     LOC2);            		\
})

#endif /* PM_INSTR_H */
