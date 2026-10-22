import psycopg
from psycopg import rows
from psycopg_pool import ConnectionPool

DSN = "host=localhost port=5432 dbname=auth user=postgres password=postgres"

pool = ConnectionPool(DSN, open=True, kwargs={"autocommit": True, "row_factory": rows.dict_row})


def execute(query, params=(), fetch=None):
    with pool.connection() as conn:
        cur = conn.execute(query, params)
        if fetch == "one":
            row = cur.fetchone()
            return row
        if fetch == "all":
            return cur.fetchall()
        return None
