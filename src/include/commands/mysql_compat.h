/*-------------------------------------------------------------------------
 *
 * mysql_compat.h
 *		MySQL-compatible SHOW statements (SHOW DATABASES / SHOW TABLES).
 *
 * These statements arrive as ordinary VariableShowStmts whose name is
 * "databases" or "tables"; guc_funcs.c routes those names here instead
 * of looking them up as GUCs.
 *
 * Portions Copyright (c) 1996-2026, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/commands/mysql_compat.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef MYSQL_COMPAT_H
#define MYSQL_COMPAT_H

#include "access/tupdesc.h"
#include "nodes/parsenodes.h"
#include "tcop/dest.h"

extern bool ShowCompatIsName(const char *name);
extern void ShowCompatExecute(const char *name, DestReceiver *dest);
extern TupleDesc ShowCompatResultDesc(const char *name);

extern void ShowCompatColumns(ShowColumnsStmt *stmt, DestReceiver *dest);
extern TupleDesc ShowCompatColumnsResultDesc(void);

#endif							/* MYSQL_COMPAT_H */
