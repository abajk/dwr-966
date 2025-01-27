/* adpTgtosCompat.h - post include for Linux */

/* Copyright (c) 2005, TeamF1, Inc. */

/*
modification history
--------------------
01a,06sep05,rnp  written
*/

#ifndef __INCadpTgtosCompatH
#define __INCadpTgtosCompatH

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* includes */
#include "adpLnxUnMap.h"

/* defines */
#ifndef ADP_IO_API_FUNCTION_TABLE
#define ADP_IO_API_FUNCTION_TABLE   adpLnxIoFuncTbl
#endif /* ADP_IO_API_FUNCTION_TABLE */

#ifndef ADP_IO_API_LIB_INIT_RTN
#define ADP_IO_API_LIB_INIT_RTN     adpLnxIoLibInstall
#endif /* ADP_IO_API_LIB_INIT_RTN */

#ifndef ADP_IO_API_LIB_INIT_ARG
#define ADP_IO_API_LIB_INIT_ARG     (NULL)
#endif /* ADP_IO_API_LIB_INIT_ARG */

/* imports */
IMPORT ADP_IO_FUNCTBL * adpLnxIoLibInstall (void *);
IMPORT ADP_IO_FUNCTBL adpLnxIoFuncTbl;

#ifdef __cplusplus
}
#endif

#endif /* __INCadpTgtosCompatH */
