/*-------------------------------------------------------------------------
 *
 * mysql_compat.c
 *		MySQL-compatible SHOW statements: SHOW DATABASES, SHOW TABLES,
 *		SHOW COLUMNS FROM tbl.
 *
 * "databases" and "tables" are plain identifiers, so those statements
 * already parse as VariableShowStmt with that name; GetPGVariable()
 * and GetPGVariableResultDesc() route the names here instead of looking
 * them up as GUCs.  SHOW COLUMNS FROM tbl is a distinct statement with
 * its own grammar production and node type.
 *
 * The semantics mirror MySQL:
 *	- SHOW DATABASES lists the databases the current role may connect to;
 *	- SHOW TABLES lists the base tables and views visible in the search
 *	  path;
 *	- SHOW COLUMNS FROM tbl lists the relation's columns in MySQL's
 *	  six-column format.
 *
 * Portions Copyright (c) 1996-2026, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *		src/backend/commands/mysql_compat.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "catalog/namespace.h"
#include "commands/mysql_compat.h"
#include "executor/executor.h"
#include "executor/spi.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"

/* The SHOW names handled as VariableShowStmt, in sorted order. */
static const char *const compat_show_names[] = {"databases", "tables"};

static const char *const show_databases_sql =
"SELECT d.datname::text AS \"Database\" "
"FROM pg_catalog.pg_database d "
"WHERE d.datallowconn "
"AND pg_catalog.has_database_privilege(d.oid, 'CONNECT') "
"ORDER BY 1";

static const char *const show_tables_sql =
"SELECT c.relname::text AS %s FROM pg_catalog.pg_class c, pg_catalog.pg_namespace n "
"WHERE c.relnamespace = n.oid "
"AND c.relkind IN ('r', 'p', 'v', 'm') "
"AND n.nspname <> 'pg_catalog' "
"AND n.nspname <> 'information_schema' "
"AND n.nspname !~ '^pg_toast' "
"AND pg_catalog.pg_table_is_visible(c.oid) "
"ORDER BY 1";

static const char *const show_columns_sql =
"SELECT a.attname::text AS \"Field\", "
"pg_catalog.format_type(a.atttypid, a.atttypmod) AS \"Type\", "
"CASE WHEN a.attnotnull THEN 'NO' ELSE 'YES' END AS \"Null\", "
"CASE WHEN i.indexrelid IS NOT NULL THEN 'PRI' ELSE '' END AS \"Key\", "
"pg_catalog.pg_get_expr(ad.adbin, ad.adrelid) AS \"Default\", "
"CASE WHEN a.attidentity <> '' THEN 'auto_increment' ELSE '' END AS \"Extra\" "
"FROM pg_catalog.pg_attribute a "
"LEFT JOIN pg_catalog.pg_attrdef ad "
"ON ad.adrelid = a.attrelid AND ad.adnum = a.attnum "
"LEFT JOIN pg_catalog.pg_index i "
"ON i.indrelid = a.attrelid AND i.indisprimary "
"AND a.attnum = ANY(i.indkey) "
"WHERE a.attrelid = $1 AND a.attnum > 0 AND NOT a.attisdropped "
"AND ($2::text IS NULL OR a.attname LIKE $2) "
"ORDER BY a.attnum";

/*
 * Does "name" belong to this module (as a VariableShowStmt name)?
 */
bool
ShowCompatIsName(const char *name)
{
	for (int i = 0; i < lengthof(compat_show_names); i++)
		if (pg_strcasecmp(name, compat_show_names[i]) == 0)
			return true;
	return false;
}

/*
 * Column header for SHOW TABLES, in MySQL's "Tables_in_<db>" style: the
 * first schema on the search path plays the role MySQL assigns to the
 * current database.
 */
static char *
show_tables_colname(void)
{
	List	   *path = fetch_search_path(false);
	char	   *nsname = NULL;

	if (path != NIL)
	{
		Oid			firstns = linitial_oid(path);

		if (OidIsValid(firstns))
			nsname = get_namespace_name(firstns);
	}
	list_free(path);

	return nsname ? psprintf("Tables_in_%s", nsname) : pstrdup("tables");
}

/*
 * Connect to SPI and run a read-only SELECT.  The result stays available
 * in SPI_tuptable until SPI_finish().
 */
static void
spi_run(const char *sql, int nargs, Oid *argtypes, Datum *values,
		const char *nulls)
{
	int			ret;

	if (SPI_connect() != SPI_OK_CONNECT)
		elog(ERROR, "SPI_connect failed");

	ret = SPI_execute_with_args(sql, nargs, argtypes, values, nulls,
								true, 0);
	if (ret != SPI_OK_SELECT)
		elog(ERROR, "SPI_execute failed: error code %d", ret);
}

/*
 * Send the current SPI result table (all rows, all columns, rendered as
 * text) to dest, using the query's output column names.
 */
static void
spi_send(DestReceiver *dest)
{
	TupOutputState *tstate;
	TupleDesc	tupdesc;
	Datum	   *values;
	bool	   *nulls;
	int			ncols;

	tupdesc = CreateTupleDescCopy(SPI_tuptable->tupdesc);
	ncols = tupdesc->natts;

	values = palloc(ncols * sizeof(Datum));
	nulls = palloc(ncols * sizeof(bool));

	tstate = begin_tup_output_tupdesc(dest, tupdesc, &TTSOpsVirtual);

	for (uint64 row = 0; row < SPI_processed; row++)
	{
		for (int col = 1; col <= ncols; col++)
		{
			char	   *val;

			val = SPI_getvalue(SPI_tuptable->vals[row],
							   SPI_tuptable->tupdesc, col);
			if (val)
			{
				values[col - 1] = PointerGetDatum(cstring_to_text(val));
				nulls[col - 1] = false;
			}
			else
			{
				values[col - 1] = (Datum) 0;
				nulls[col - 1] = true;
			}
		}
		do_tup_output(tstate, values, nulls);
	}

	end_tup_output(tstate);
	SPI_finish();
}

/*
 * Execute a MySQL-compatible SHOW statement named by a VariableShowStmt.
 */
void
ShowCompatExecute(const char *name, DestReceiver *dest)
{
	char	   *sql;

	Assert(ShowCompatIsName(name));	/* caller checked */

	if (pg_strcasecmp(name, "databases") == 0)
		sql = pstrdup(show_databases_sql);
	else
		sql = psprintf(show_tables_sql,
					   quote_identifier(show_tables_colname()));

	spi_run(sql, 0, NULL, NULL, NULL);
	spi_send(dest);
}

/*
 * Result descriptor for a MySQL-compatible SHOW statement named by a
 * VariableShowStmt.
 */
TupleDesc
ShowCompatResultDesc(const char *name)
{
	TupleDesc	tupdesc = CreateTemplateTupleDesc(1);

	Assert(ShowCompatIsName(name));	/* caller checked */

	if (pg_strcasecmp(name, "databases") == 0)
		TupleDescInitBuiltinEntry(tupdesc, (AttrNumber) 1, "Database",
								  TEXTOID, -1, 0);
	else
		TupleDescInitBuiltinEntry(tupdesc, (AttrNumber) 1,
								  show_tables_colname(),
								  TEXTOID, -1, 0);
	return tupdesc;
}

/*
 * Execute SHOW COLUMNS FROM tbl.
 */
void
ShowCompatColumns(ShowColumnsStmt *stmt, DestReceiver *dest)
{
	Oid			relid;
	Oid			argtypes[2] = {OIDOID, TEXTOID};
	Datum		values[2];
	char		nulls[2] = {' ', ' '};

	/* report a standard error if the relation does not exist */
	relid = RangeVarGetRelid(stmt->relation, NoLock, false);

	values[0] = ObjectIdGetDatum(relid);
	if (stmt->pattern)
		values[1] = PointerGetDatum(cstring_to_text(stmt->pattern));
	else
		nulls[1] = 'n';

	spi_run(show_columns_sql, 2, argtypes, values, nulls);
	spi_send(dest);
}

/*
 * Result descriptor for SHOW COLUMNS (static: six text columns).
 */
TupleDesc
ShowCompatColumnsResultDesc(void)
{
	static const char *const colnames[] = {
		"Field", "Type", "Null", "Key", "Default", "Extra"
	};
	TupleDesc	tupdesc = CreateTemplateTupleDesc(lengthof(colnames));

	for (int i = 0; i < lengthof(colnames); i++)
		TupleDescInitBuiltinEntry(tupdesc, (AttrNumber) (i + 1),
								  colnames[i], TEXTOID, -1, 0);
	return tupdesc;
}
