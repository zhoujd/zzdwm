# Cscope Hardening

Status: implemented.

MicroEMACS supports cscope on Linux, macOS, Cygwin, and FreeBSD. The editor
starts cscope in line mode with `cscope -l -k`, optionally adds `-d` when
database updates are disabled, and feeds matches into the shared tag ring.
`find-cscope` is bound to **M-,**, `find-grep` is bound to **M-G**, and
`next-cscope` is available as an unbound command.

## Hardening

The cscope response parser now:

- validates the `>> ` prompt before sending a query;
- checks pipe writes and flushes for errors;
- rejects missing, malformed, or negative match counts;
- parses filenames, enclosing symbols, and line numbers with bounded widths;
- rejects incomplete or oversized response fields;
- stops instead of reusing stale values after a malformed response.

The Unix two-way pipe now checks `fork()`, closes all pipe descriptors on
every failure path, exits the child after `dup2()` or `execvp()` failure, and
clears output pointers before reporting failure. Expected duplicate tag
entries are returned silently because cscope definition and reference queries
can naturally contain the same file and line.

## Testing

`test/cscope.c` covers valid matches, `<global>` matches, malformed line
numbers, incomplete responses, and oversized fields. It also checks that a
missing cscope executable produces a clean search failure.

A real database can be tested with a temporary fixture:

```sh
cd tools/MicroEMACS
make -C test cscope-test

tmpdir=$(mktemp -d)
printf '%s\n' sample.c > "$tmpdir/cscope.files"
cat > "$tmpdir/sample.c" <<'EOF'
int sample_value = 42;

int sample_function(int input)
{
  return input + sample_value;
}
EOF

cd "$tmpdir"
cscope -b -k -i cscope.files
/path/to/microemacs/test/cscope-test sample_function sample_value
```

## Limitations

The Windows/MinGW build still does not implement the two-way pipe required by
cscope, so cscope commands remain disabled there. Cscope stderr is redirected
to `/dev/null` to protect the editor display; a failed query is reported as a
cscope search failure rather than exposing the child process diagnostic.
