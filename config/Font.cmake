# Shared fixed-width bitmap font settings.
# Glyph rows are stored top-to-bottom and most-significant bit first.

set(TABOS_FONT_FILE "${CMAKE_CURRENT_LIST_DIR}/../graphics/blueterm.f12" CACHE FILEPATH
    "Raw fixed-width bitmap font file")
set(TABOS_FONT_GLYPH_WIDTH 8 CACHE STRING "Font glyph width in pixels")
set(TABOS_FONT_GLYPH_HEIGHT 12 CACHE STRING "Font glyph height in pixels")
set(TABOS_FONT_GLYPH_COUNT 256 CACHE STRING "Number of glyphs in the font")
set(TABOS_FONT_CELL_WIDTH 8 CACHE STRING "Terminal font cell width in pixels")
set(TABOS_FONT_CELL_HEIGHT 15 CACHE STRING "Terminal font cell height in pixels")

foreach(value_name IN ITEMS
        TABOS_FONT_GLYPH_WIDTH
        TABOS_FONT_GLYPH_HEIGHT
        TABOS_FONT_GLYPH_COUNT
        TABOS_FONT_CELL_WIDTH
        TABOS_FONT_CELL_HEIGHT)
    if(NOT ${value_name} MATCHES "^[1-9][0-9]*$")
        message(FATAL_ERROR "${value_name} must be a positive integer")
    endif()
endforeach()

if(TABOS_FONT_GLYPH_COUNT GREATER 256)
    message(FATAL_ERROR "TABOS_FONT_GLYPH_COUNT must not exceed 256")
endif()
if(TABOS_FONT_CELL_WIDTH LESS TABOS_FONT_GLYPH_WIDTH OR
   TABOS_FONT_CELL_HEIGHT LESS TABOS_FONT_GLYPH_HEIGHT)
    message(FATAL_ERROR "font cell dimensions must contain the configured glyph dimensions")
endif()
if(NOT EXISTS "${TABOS_FONT_FILE}")
    message(FATAL_ERROR "configured font file does not exist: ${TABOS_FONT_FILE}")
endif()

math(EXPR TABOS_FONT_BYTES_PER_ROW "(${TABOS_FONT_GLYPH_WIDTH} + 7) / 8")
math(EXPR TABOS_FONT_EXPECTED_SIZE
    "${TABOS_FONT_GLYPH_COUNT} * ${TABOS_FONT_GLYPH_HEIGHT} * ${TABOS_FONT_BYTES_PER_ROW}")
file(SIZE "${TABOS_FONT_FILE}" TABOS_FONT_FILE_SIZE)
if(NOT TABOS_FONT_FILE_SIZE EQUAL TABOS_FONT_EXPECTED_SIZE)
    message(FATAL_ERROR
        "font file size is ${TABOS_FONT_FILE_SIZE} bytes; expected ${TABOS_FONT_EXPECTED_SIZE} "
        "for ${TABOS_FONT_GLYPH_COUNT} glyphs of ${TABOS_FONT_GLYPH_WIDTH}x${TABOS_FONT_GLYPH_HEIGHT}")
endif()
