#ifndef KILO_RENDER_H
#define KILO_RENDER_H
#include <kilo/editor.h>
#include <sys/types.h>
typedef ssize_t (*kilo_output_fn)(int, const void*, size_t);
bool kilo_render(kilo_editor_t* editor, kilo_output_fn output);
#endif
