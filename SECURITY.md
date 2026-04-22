# Security Policy

## Supported Versions

Security handling currently applies to the latest public release and the `main` branch.

## Reporting a Vulnerability

Please do not open a public issue for vulnerabilities involving unsafe parsing, crafted firmware images, path traversal, memory corruption, or accidental disclosure of private firmware data.

Use GitHub's private vulnerability reporting if it is available for the repository, or contact the maintainer through GitHub with:

- A clear description of the issue.
- Reproduction steps.
- A minimal non-sensitive sample when possible.
- Your assessment of impact.

## Firmware Safety

This project does not flash firmware. Still, malformed parsing or profile generation can lead users toward unsafe output. Treat parsing bugs, incorrect offsets, checksum mistakes, and misleading UI behavior as safety-sensitive.
