# Move QuickQuill Postgres from Render to Neon

This moves users, sessions, documents, search history, and suggested words.
The C++ engine's `dictionary.db` stays in the Docker image. The backend stays
on Render and the frontend stays on Vercel.

## Prepare

1. Create an empty Neon database, preferably near the Render backend in Oregon.
   Match the source Postgres major version. Record both server versions with
   `SHOW server_version;`. Use a `pg_dump` client at least as new as the source
   server and a matching `pg_restore` client.
2. Get Render's **external** database URL and Neon's **direct/unpooled** URL
   from their dashboards. Both must use TLS (`sslmode=require`). The Neon URL
   for these CLI commands starts with `postgresql://`, not `jdbc:`.
3. Preserve the existing Render `DB_URL`, `DB_USERNAME`, `DB_PASSWORD`, and
   `DB_SSLMODE` privately for rollback. Do not commit credentials or dump files.
4. Schedule a brief outage. Suspend the backend and stop any other writers
   before the final export. Keep them stopped until restoration and data
   checks finish. A snapshot dump alone does not capture subsequent writes.
5. Keep the destination empty until restore completes. Starting the app early
   would let Hibernate's `ddl-auto=update` create conflicting tables.

## Copy and verify

Run the following in **Bash**, with shell tracing disabled. Hidden prompts keep
URLs out of shell history; `pg_dump` and `psql` read them from `PGDATABASE`.
Use a private directory outside the repository for the dump and verification
files. Run each stage only if the previous stage succeeds.

```bash
set +x
umask 077
read -rsp 'Render external Postgres URL: ' RENDER_DATABASE_URL; echo
read -rsp 'Neon direct Postgres URL: ' NEON_DATABASE_URL; echo
export PGCONNECT_TIMEOUT=30
export PGSSLMODE=require
MIGRATION_DIR=$(mktemp -d "${TMPDIR:-/tmp}/quickquill-migration.XXXXXX")
printf 'Migration files: %s\n' "$MIGRATION_DIR"

PGDATABASE="$RENDER_DATABASE_URL" pg_dump \
  --format=custom --no-owner --no-acl \
  --file="$MIGRATION_DIR/render.dump"
```

Check the dump command exited successfully and inspect its object inventory:

```bash
pg_restore --list "$MIGRATION_DIR/render.dump"
```

Restore into the empty Neon database. `--single-transaction` and
`--exit-on-error` prevent a failed restore from leaving a partial import.
Do not add `--clean` to overwrite an existing database.

```bash
pg_restore \
  --no-owner --no-acl --exit-on-error --single-transaction \
  --dbname="$NEON_DATABASE_URL" "$MIGRATION_DIR/render.dump"
```

The restore command above passes the destination URL as a command argument
because `pg_restore` requires `--dbname` to connect. On a shared workstation,
use a private libpq service file and pass `--dbname='service=quickquill_neon'`
instead to keep the password out of the process list.

Compare exact counts for **all** application tables and the state of **all**
application sequences while the source is still frozen:

```bash
cat > "$MIGRATION_DIR/verify.sql" <<'SQL'
SELECT format('SELECT %L AS object, count(*) AS rows FROM %I.%I;',
              schemaname || '.' || tablename, schemaname, tablename)
FROM pg_tables
WHERE schemaname NOT IN ('pg_catalog', 'information_schema')
ORDER BY schemaname, tablename
\gexec
SELECT format('SELECT %L AS object, last_value, is_called FROM %I.%I;',
              schemaname || '.' || sequencename, schemaname, sequencename)
FROM pg_sequences
WHERE schemaname NOT IN ('pg_catalog', 'information_schema')
ORDER BY schemaname, sequencename
\gexec
SQL
PGDATABASE="$RENDER_DATABASE_URL" psql -X -A -t -v ON_ERROR_STOP=1 \
  -f "$MIGRATION_DIR/verify.sql" > "$MIGRATION_DIR/render-counts.txt"
PGDATABASE="$NEON_DATABASE_URL" psql -X -A -t -v ON_ERROR_STOP=1 \
  -f "$MIGRATION_DIR/verify.sql" > "$MIGRATION_DIR/neon-counts.txt"
diff -u "$MIGRATION_DIR/render-counts.txt" "$MIGRATION_DIR/neon-counts.txt"
```

Stop if either query fails or the comparison differs. Also inspect the restored
schema, constraints, and indexes and compare representative records privately.
Counts alone cannot prove that every value matches. Refresh planner statistics:

```bash
PGDATABASE="$NEON_DATABASE_URL" psql -X -v ON_ERROR_STOP=1 -c 'ANALYZE;'
unset RENDER_DATABASE_URL NEON_DATABASE_URL
```

## Switch the backend

Sync the updated `render.yaml` so future Blueprint syncs no longer restore
Render database references. Local edits must first be committed and pushed
to the linked repository; pushing main also triggers the backend deployment
workflow. Coordinate this with the write freeze. Review the Blueprint sync
preview before applying it. Retain the old Render database and the dump.

Set these environment variables on `quickquill-backend`:

```dotenv
DB_URL=jdbc:postgresql://<direct-neon-host>:5432/<database>?sslmode=require
DB_USERNAME=<neon-role>
DB_PASSWORD=<neon-password>
DB_SSLMODE=require
```

Use the direct endpoint initially: this app already uses Hikari connection
pooling and runs Hibernate schema updates at startup. Supply username and
password separately; the existing URL normalizer discards credentials embedded
in a `postgresql://user:password@host/db` URL. Use `DB_URL`, not `DATABASE_URL`.

For an existing Render service, `sync: false` does **not** replace old values or
prompt for new ones. Set all three connection variables explicitly in its
Environment page. Save and deploy, then resume the backend if suspended.
Check for any `SPRING_DATASOURCE_*` overrides that would supersede `DB_*`.

Verify startup logs, then test login with an existing account, opening an
existing document, saving and reloading a test document, and search history.
Check a dictionary lookup as well. `/api/health` reports liveness only and
does not query Postgres. Resume normal traffic only after these checks pass.

## Rollback

Before any new Neon writes, restore the saved Render connection variables and
redeploy/resume. Once Neon has accepted writes, stop writers and reconcile or
copy those changes back before switching; a blind rollback loses those writes.
Keep the source database and a durable private copy of the dump until the
migration has been accepted. Database deletion is a separate manual action.

References: [Neon migration guide](https://neon.com/docs/import/migrate-from-render),
[Java connections](https://neon.com/docs/guides/java),
[Render secret variables](https://render.com/docs/blueprint-spec#prompting-for-secret-values).
