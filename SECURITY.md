# Security policy

OpenKal 0.1.x is an experimental source release. Security maintenance applies to
the current 0.1.x source and project-published artifacts; older snapshots do not
have a separate maintenance branch. Dedicated multiplayer is experimental and
must not be presented as a production-ready service.

Report suspected vulnerabilities privately to **dev@silentius.com**. Include the
affected version, build options, a minimal reproduction using synthetic data
where possible, and the observed impact. Do not put credentials, private keys,
personal data or proprietary game files in a report or public issue. Coordinate
public disclosure after the issue has been investigated and a correction is
available. No response-time guarantee is currently offered.

Pull-request review checks the origin and redistribution permissions of
contributed code, data and assets. Project review covers this repository and
artifacts published by OpenKal. The resource packer's support for external
inputs is intentional; users are responsible for their local inputs and packages.

For the current transport guarantees and limits, see
[TCP security](docs/TcpSecurity.md). TLS authenticates the server and protects
transport; gameplay validation, account authorization and persistence remain
separate server responsibilities. See the
[release notes](docs/ReleaseNotes-0.1.0.md) for known limitations.
