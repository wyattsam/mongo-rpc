/*-
 * Copyright (c) 2014-2016 MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "wt_internal.h"

static const int windows_error_offset = -29000;

/*
 * __wt_map_error_to_windows_error --
 *	Return a negative integer, an encoded Windows error
 * Standard C errors are positive integers from 0 - ~200
 * Windows errors are from 0 - 15999 according to the documentation
 */
static DWORD
__wt_map_error_to_windows_error(int error)
{
	/*
	 * Ensure we do not exceed the error range
	 * Also validate we do not get any COM errors
	 * (which are negative integers)
	 */
	WT_ASSERT(NULL, error < 0);

	return (error + -(windows_error_offset));
}

/*
 * __wt_map_windows_error_to_error --
 *	Return a positive integer, a decoded Windows error
 */
static int
__wt_map_windows_error_to_error(DWORD winerr)
{
	return (winerr + windows_error_offset);
}

/*
 * __wt_map_error_rdonly --
 *	Map an error into a  WiredTiger error code specific for
 *	read-only operation which intercepts based on certain types
 *	of failures.
 */
int
__wt_map_error_rdonly(int error)
{
	if (error == ERROR_FILE_NOT_FOUND)
		return (WT_NOTFOUND);
	else if (error == ERROR_ACCESS_DENIED)
		return (WT_PERM_DENIED);
	return (error);
}

/*
 * __wt_errno --
 *	Return errno, or WT_ERROR if errno not set.
 */
int
__wt_errno(void)
{
	/*
	 * Check for 0:
	 * It's easy to introduce a problem by calling the wrong error function,
	 * for example, this function when the MSVC function set the C runtime
	 * error value. Handle gracefully and always return an error.
	 */
	return (errno == 0 ? WT_ERROR : errno);
}

/*
 * __wt_getlasterror --
 *	Return GetLastError, or WT_ERROR if error not set.
 */
int
__wt_getlasterror(void)
{
	/*
	 * Called when we know an error occurred, and we want the system
	 * error code.
	 */
	DWORD err = GetLastError();

	/*
	 * Check for ERROR_SUCCESS:
	 * It's easy to introduce a problem by calling the wrong error function,
	 * for example, this function when the MSVC function set the C runtime
	 * error value. Handle gracefully and always return an error.
	 */
	return (err == ERROR_SUCCESS ?
	    WT_ERROR : __wt_map_windows_error_to_error(err));
}

/*
 * __wt_strerror --
 *	Windows implementation of WT_SESSION.strerror and wiredtiger_strerror.
 */
const char *
__wt_strerror(WT_SESSION_IMPL *session, int error, char *errbuf, size_t errlen)
{
	DWORD lasterror;
	const char *p;
	char buf[512];

	/*
	 * Check for a WiredTiger or POSIX constant string, no buffer needed.
	 */
	if ((p = __wt_wiredtiger_error(error)) != NULL)
		return (p);

	/*
	 * When called from wiredtiger_strerror, write a passed-in buffer.
	 * When called from WT_SESSION.strerror, write the session's buffer.
	 *
	 * Check for Windows errors.
	 */
	if (error < 0) {
		error = __wt_map_error_to_windows_error(error);

		lasterror = FormatMessageA(
			FORMAT_MESSAGE_FROM_SYSTEM |
			    FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			error,
			0, /* let system choose the correct LANGID */
			buf,
			sizeof(buf),
			NULL);

		if (lasterror != 0 && session == NULL &&
		    snprintf(errbuf, errlen, "%s", buf) > 0)
			return (errbuf);
		if (lasterror != 0 && session != NULL &&
		    __wt_buf_fmt(session, &session->err, "%s", buf) == 0)
			return (session->err.data);
	}

	/* Fallback to a generic message. */
	if (session == NULL &&
	    snprintf(errbuf, errlen, "error return: %d", error) > 0)
		return (errbuf);
	if (session != NULL && __wt_buf_fmt(
	    session, &session->err, "error return: %d", error) == 0)
		return (session->err.data);

	/* Defeated. */
	return ("Unable to return error string");
}
