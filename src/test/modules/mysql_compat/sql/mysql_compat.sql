--
-- MYSQL_COMPAT
-- MySQL-compatible SHOW statements: SHOW DATABASES, SHOW TABLES,
-- SHOW COLUMNS (and the DESCRIBE synonym).
--
-- The tests run inside a dedicated schema so that the visible-table list
-- does not depend on leftovers from other test files.
--

CREATE SCHEMA mysql_compat_sc;
SET search_path = mysql_compat_sc;

--
-- SHOW DATABASES
--
-- Lists the databases the current role may connect to.  template0 has
-- datallowconn = false and is therefore hidden.
--
SHOW DATABASES;

--
-- SHOW TABLES
--
-- Lists the relations visible in the current search path (base tables and
-- views, mirroring MySQL's SHOW TABLES).
--
CREATE TABLE mysql_compat_t1 (a int);
CREATE TABLE mysql_compat_t2 (b text);
CREATE VIEW mysql_compat_v1 AS SELECT 1;

SHOW TABLES;

-- Tables in schemas that are not on the search path stay hidden.
CREATE SCHEMA mysql_compat_other;
CREATE TABLE mysql_compat_other.hidden_t (c int);

SHOW TABLES;

SET search_path = mysql_compat_sc, mysql_compat_other;

SHOW TABLES;

RESET search_path;

--
-- USE was deliberately not implemented: PostgreSQL cannot switch to
-- another database within a session, so it stays a plain syntax error
-- rather than growing a statement that only reports an error.
--
USE postgres;

--
-- SHOW COLUMNS
--
-- Lists the columns of a relation in MySQL's six-column format.
--
SET search_path = mysql_compat_sc;

CREATE TABLE columns_t (
    id   int GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    name text NOT NULL,
    note text,
    val  numeric(10,2) DEFAULT 0
);

SHOW COLUMNS FROM columns_t;

-- schema-qualified spelling works too
SHOW COLUMNS FROM mysql_compat_sc.columns_t;

-- missing relations report the usual error
SHOW COLUMNS FROM mysql_compat_sc.no_such_table;

-- LIKE filters the column names
SET search_path = mysql_compat_sc;
SHOW COLUMNS FROM columns_t LIKE 'n%';

-- DESCRIBE is a synonym for SHOW COLUMNS FROM
DESCRIBE columns_t;

RESET search_path;
