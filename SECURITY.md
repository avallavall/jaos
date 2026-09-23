# Security

## Reporting

Report a vulnerability privately, through the repository's GitHub page:
the Security tab, "Report a vulnerability". Do not open a public issue for
it. Include the input that shows it and the version `jaos --version`
prints. The version names the last tagged release, so for a build from the
repository include its commit too (`git rev-parse --short HEAD`).

## What is exposed

JAOS reads files it did not write: MPS, LP, `.nl`, OSiL, QPLIB and CBF
models, gzip-compressed or not, and option files. It also reads the
solution, basis, certificate, cone-duals and proof files its own writers
produce (`jaos_read_certificate`, `jaos_read_cone_duals`,
`jaos_check_proof` and `jaos check FILE --proof`), point and duals files,
and MPS basis files (`jaos_read_mps_basis` and `--basis`). A point, duals
or MPS basis file may come from another solver. Those readers are the part
that takes untrusted input, and a crash, a hang or a read or write outside
a buffer on some input is a vulnerability. Every reader is meant to refuse
what it cannot hold with a message that names the line.

The solver works on a model already in memory. A model that makes it run
for a long time is not a vulnerability: `jaos_set_work_limit` and
`--work-limit` bound every solve.

## Supported versions

Fixes land on `main` and in the next release. Only the latest release is
supported.
