# PostgreSQL development database

The multiplayer development database uses the official
[`postgres:18` image](https://hub.docker.com/_/postgres). Docker Compose is not
required. PostgreSQL 18 stores its data beneath `/var/lib/postgresql`, which is
mounted as a persistent named volume.

Run these commands from the repository root. Create `save/postgres.env` if it
does not already exist, with a locally chosen password:

```dotenv
POSTGRES_USER=openkal
POSTGRES_DB=openkal
POSTGRES_PASSWORD=replace-with-a-local-password
```

Keep that file private (`chmod 600 save/postgres.env`). The `save/` directory is
ignored by Git. Image initialization settings apply only to an empty data
volume; editing this file does not change an existing database password.

```bash
docker pull postgres:18
docker run -d --name openkal-postgres \
  --restart unless-stopped \
  --env-file save/postgres.env \
  --publish 127.0.0.1:5432:5432 \
  --volume openkal-postgres-data:/var/lib/postgresql \
  --health-cmd 'pg_isready -U openkal -d openkal' \
  --health-interval 5s --health-timeout 5s --health-retries 12 \
  postgres:18
```

After the first creation, use `docker stop openkal-postgres` and
`docker start openkal-postgres`. The named volume retains the data when the
container is replaced. Do not remove `openkal-postgres-data` if the data is needed.

The database is reachable on `127.0.0.1:5432`, with database and user `openkal`.
Use the password from `save/postgres.env` for TCP connections. To open a local SQL
console inside the container:

```bash
docker exec -it openkal-postgres psql -U openkal -d openkal
```

This prepares the database service. Build OpenKal with `OPENKAL_WITH_POSTGRES=ON`
and follow [Multiplayer setup](Multiplayer.md) to start MainSvr and two clients.
MainSvr reads this credentials file directly for local development; no Python
launcher or exported database connection string is required. MainSvr creates and
validates its PostgreSQL schema. Game clients connect to
MainSvr, which owns database access. Single Player continues to use SQLite.
