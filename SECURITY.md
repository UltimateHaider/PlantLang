# Security Policy

PlantLang / Chloroplast is a self-hosted programming language project. We take
security vulnerabilities seriously and appreciate your help in reporting them
responsibly.

## Reporting a Vulnerability

**Please do not open a public GitHub issue for security vulnerabilities.**

Instead, report vulnerabilities privately through one of these channels:

1. **GitHub private vulnerability reporting** — use the *Security* tab →
   *Report a vulnerability* on the repository
   (`https://github.com/UltimateHaider/PlantLang/security/advisories`).
2. **Email** — `[INSERT SECURITY CONTACT EMAIL]` with the subject line
   `[SECURITY] <short description>`.

Please include as much of the following as possible:

- The affected version(s) of the compiler and/or runtime
- A description of the vulnerability and its potential impact
- Steps to reproduce it (preferably a minimal `.plant` source file)
- The actual output/behavior vs. the expected behavior
- Any relevant environment details (OS, compiler flags, architecture)

You will receive an acknowledgement of your report within **48 hours**, and a
status update at least every **7 days** until the issue is resolved.

## Supported Versions

Only the latest release line receives security fixes. Older release lines are
considered end-of-life.

| Version | Supported          |
| ------- | ------------------ |
| 0.49.x  | :white_check_mark: |
| <= 0.48 | :x:                |

## Disclosure Policy

We follow a coordinated disclosure process:

1. **Confirmation** — the report is acknowledged and triaged.
2. **Investigation** — a fix is developed, and a release containing the fix is
   prepared (typically on the current release line; backports to older lines
   are considered on a case-by-case basis).
3. **Disclosure** — once a fixed release is published, the vulnerability is
   disclosed publicly (release notes, advisory, and/or CVE assignment where
   appropriate), and the reporter is credited unless they prefer otherwise.

We aim to publish fixes promptly and to disclose vulnerabilities only after a
fixed version is available, unless the vulnerability is already being
exploited in the wild — in which case we may disclose earlier to protect users.