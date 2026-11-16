# ADR-0001: Engine backend choice

Status: accepted. Decision date: 2026-08-07.

OpenKal uses `engines/gl` as its interactive backend: GLFW 3 window/input,
GLEW and OpenGL 3.3 core. `engines/null` remains available for headless tests.
Game and protocol logic stay independent of backend headers and resources.

Shaders use GLSL. A future Godot backend would implement the engine contracts;
Godot is not required by the current build. HackShield and HsCrypt are outside
project scope. The optional `IPacketCipher` protocol transform is separate from
[TLS transport security](../TcpSecurity.md).

See [Architecture](../ARCHITECTURE.md) for module boundaries and the
[project README](../../README.md) for runtime asset locations and build options.
