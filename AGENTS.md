# AGENTS.md

Before making architectural, platform, build, or testing changes, read:

- `agents/TABOS_CONTEXT.md`
- `agents/architecture.md`
- `agents/testing.md`
- `agents/roadmap.md`

Treat decisions marked `[DECIDED]` as project requirements unless
the user explicitly asks to reconsider them.

Preserve the platform boundaries, multi-target build strategy, and testing
requirements defined in these documents when implementing or modifying code.

The `agents/` directory provides implementation context for coding agents. It is
not user or contributor documentation.

User and contributor documentation lives in `docs/`. Update the relevant
document whenever a change affects documented structure, commands, behavior,
requirements, or workflows.
