import pathlib
import pytest
from testsuite.databases.pgsql import discover

pytest_plugins = ['pytest_userver.plugins.postgresql']


@pytest.fixture(scope='session')
def pgsql_local(pgsql_local_create, service_source_dir):
    """Session-scoped PostgreSQL fixture required by uServer testsuite."""
    databases = discover.find_schemas(
        None,
        schema_dirs=[pathlib.Path(service_source_dir) / 'postgresql' / 'schemas'],
    )
    return pgsql_local_create(list(databases.values()))
