# TabOS C Coding Style

> Status: required coding-agent conventions.
>
> Purpose: keep generated and modified C code explicit, consistently scoped, and
> easy to review.

## Control-Flow Scope

Every control-flow body must use braces, including bodies containing only one
statement. This applies to `if`, `else`, `for`, `while`, and `do` statements.
`switch` bodies must remain explicitly braced and readable. Do not emit nested or
trailing unbraced one-line control flow.

Required:

```c
for (unsigned int index = 0U; index < STARFALL_SHOT_COUNT; ++index) {
    if (game->shots[index].active) {
        (void)tabos_graphics_fill_rect(
            graphics,
            game->shots[index].x,
            game->shots[index].y,
            3U,
            8U,
            COLOR_YELLOW
        );
    }
}
```

Not allowed:

```c
for (unsigned int index = 0U; index < STARFALL_SHOT_COUNT; ++index)
    if (game->shots[index].active)
        (void)tabos_graphics_fill_rect(graphics, game->shots[index].x,
            game->shots[index].y, 3U, 8U, COLOR_YELLOW);
```

Use braces for an `else` body even when the corresponding `if` body is already
braced. Keep each control statement and its body visually distinct. Do not place
multiple control-flow statements or unrelated operations on one physical line.

## Conditional Expressions

Use the ternary operator only when both possible results are short, simple values
and the expression is easier to read than an `if`/`else` statement. A ternary is
also acceptable when measurement shows it is a necessary optimization over a
longer branch chain and the reason is documented.

Do not use nested ternaries. Do not use ternaries for multi-step behavior, side
effects, assignments that need explanation, or expressions that become difficult
to scan after line wrapping. Use a properly braced `if`/`else` chain instead.

Acceptable:

```c
const uint32_t scale = horizontal_scale < vertical_scale
    ? horizontal_scale
    : vertical_scale;
```

Prefer `if`/`else` for more involved decisions:

```c
if (rotation == TABOS_GRAPHICS_ROTATE_90) {
    source_x = source_width - 1U - rotated_y;
    source_y = rotated_x;
} else if (rotation == TABOS_GRAPHICS_ROTATE_180) {
    source_x = source_width - 1U - rotated_x;
    source_y = source_height - 1U - rotated_y;
}
```

## Readability

Prefer explicit intermediate variables and clearly scoped blocks over compressed
expressions. Line wrapping must not make several logical statements appear to be
one statement. Optimize for reviewability first; use compact forms only when they
remain immediately understandable.

## Formatting

The repository-root `.clang-format` is based on the Zeal Development Environment
format profile, with mandatory brace insertion and one-line loop suppression added
to enforce this document. Use this file for all C formatting. Do not introduce a
different directory-local format profile.

VS Code C/C++ format commands discover the root profile automatically. Formatting
must preserve required braces; if an installed formatter does not support
`InsertBraces`, update the formatter instead of removing or commenting out the rule.

Use `// clang-format off` and `// clang-format on` narrowly around intentionally
hand-aligned data tables, embedded protocol layouts, or similar structures where the
visual arrangement carries meaning. Do not disable formatting for ordinary control
flow or whole files.
