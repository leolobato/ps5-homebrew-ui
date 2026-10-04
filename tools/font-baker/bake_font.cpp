// ps5-homebrew-ui - Offline SDF font atlas baker (host tool).
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// usage: bake_font <font.ttf|.ttc> <out.huifont> [pixel_size=56] [sdf_range=8] [atlas=1024]
//                  [glyphs=basic] [face index=0]
// Bakes printable ASCII plus a few UI symbols, and whatever the glyph list
// adds, into a single-channel signed distance field atlas with metrics and
// kerning (see src/gfx/font_format.hpp).
//
// The glyph list is items separated by commas:
//   basic                 nothing more (the default)
//   european              accented Latin, Greek, Cyrillic, quotes, euro, trade mark
//   U+2122, U+2000-206F   one code point or a range (hexadecimal)
//   text=<file>           every character of a UTF-8 text file
//   @<file>               more items, one or more per line; '#' starts a comment
//   all                   every character the font maps (CJK: tens of thousands)

#define STB_TRUETYPE_IMPLEMENTATION
#include "../../third_party/stb/stb_truetype.h"

#include "../../src/gfx/font_format.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{

namespace ff = hui::gfx::font_format;

struct Baked
{
    int codepoint = 0;
    int w = 0;
    int h = 0;
    int xoff = 0;
    int yoff = 0;
    float advance = 0.0f;
    unsigned char *bitmap = nullptr;
    int x = 0;
    int y = 0;
};

// european adds the letters of western and central European languages,
// modern Greek and basic Cyrillic (Latin-1 Supplement, Latin Extended-A,
// U+0386 to U+03CE, U+0410 to U+044F, Io), curly quotation marks, the euro
// and the trade mark sign: about 450 more glyphs, which need a 2048 atlas at
// 56 pixels. A face without Greek (Montserrat) simply skips those letters.
void add_european(std::set<int> &result)
{
    for (int c = 0x00A1; c <= 0x017F; ++c)
        result.insert(c);
    // Greek with its tonos accents and dialytika; three code points in
    // the range are not assigned.
    for (int c = 0x0386; c <= 0x03CE; ++c)
    {
        if (c != 0x0387 && c != 0x038B && c != 0x038D && c != 0x03A2)
            result.insert(c);
    }
    for (int c = 0x0410; c <= 0x044F; ++c)
        result.insert(c);
    for (const int c : {0x0401, 0x0451, 0x2018, 0x2019, 0x201C, 0x201D, 0x201E, 0x20AC, 0x2122})
        result.insert(c);
}

// Decodes UTF-8 leniently: a malformed byte is skipped.
void add_text(std::set<int> &result, const std::string &text)
{
    for (std::size_t i = 0; i < text.size();)
    {
        const auto lead = static_cast<unsigned char>(text[i]);
        const int length = lead < 0x80 ? 1 : lead >= 0xf0 ? 4 : lead >= 0xe0 ? 3 : lead >= 0xc0 ? 2 : 0;
        if (length == 0 || i + static_cast<std::size_t>(length) > text.size())
        {
            ++i;
            continue;
        }
        int value = length == 1 ? lead : lead & (0x7f >> length);
        for (int k = 1; k < length; ++k)
            value = (value << 6) | (static_cast<unsigned char>(text[i + static_cast<std::size_t>(k)]) & 0x3f);
        if (value >= 32)
            result.insert(value);
        i += static_cast<std::size_t>(length);
    }
}

std::string read_all(const std::string &path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

int parse_hex(const std::string &value)
{
    std::string digits = value;
    if (digits.rfind("U+", 0) == 0 || digits.rfind("u+", 0) == 0)
        digits = digits.substr(2);
    return static_cast<int>(std::strtol(digits.c_str(), nullptr, 16));
}

bool add_items(std::set<int> &result, const std::string &list, const stbtt_fontinfo &font, int depth)
{
    std::string item;
    std::stringstream items(list);
    while (std::getline(items, item, ','))
    {
        item.erase(0, item.find_first_not_of(" \t\r\n"));
        item.erase(item.find_last_not_of(" \t\r\n") + 1);
        if (item.empty() || item == "basic")
            continue;
        if (item == "european")
        {
            add_european(result);
        }
        else if (item == "all")
        {
            // stb_truetype cannot walk a cmap, so every plane in use is probed.
            for (int c = 0x20; c <= 0x3FFFF; ++c)
            {
                if (stbtt_FindGlyphIndex(&font, c) != 0)
                    result.insert(c);
            }
        }
        else if (item.rfind("text=", 0) == 0)
        {
            add_text(result, read_all(item.substr(5)));
        }
        else if (item[0] == '@' && depth < 4)
        {
            std::string lines;
            std::stringstream file(read_all(item.substr(1)));
            for (std::string line; std::getline(file, line);)
            {
                line = line.substr(0, line.find('#'));
                for (char &c : line)
                {
                    if (c == ' ' || c == '\t')
                        c = ',';
                }
                lines += line + ",";
            }
            if (!add_items(result, lines, font, depth + 1))
                return false;
        }
        else if (item[0] == 'U' || item[0] == 'u')
        {
            const std::size_t dash = item.find('-');
            const int first = parse_hex(item.substr(0, dash));
            const int last = dash == std::string::npos ? first : parse_hex(item.substr(dash + 1));
            for (int c = first; c <= last && c <= 0x10FFFF; ++c)
                result.insert(c);
        }
        else
        {
            std::fprintf(stderr, "unknown glyph item '%s'\n", item.c_str());
            return false;
        }
    }
    return true;
}

std::vector<int> codepoints(const std::set<int> &more)
{
    std::set<int> result(more);
    for (int c = 32; c < 127; ++c)
        result.insert(c);
    // Middle dot, multiplication sign, copyright, degree, en/em dash, bullet,
    // ellipsis, arrows, check mark, and (in fonts that have them) a full
    // block, a black circle and four pointing triangles. Glyphs a font lacks
    // are skipped; test with Font::has_glyph before relying on one.
    for (const int c : {0x00B7, 0x00D7, 0x00A9, 0x00B0, 0x2013, 0x2014, 0x2022, 0x2026, 0x2190,
                        0x2191, 0x2192, 0x2193, 0x2713, 0x2588, 0x25CF, 0x25B2, 0x25B6, 0x25BC,
                        0x25C0})
        result.insert(c);
    return std::vector<int>(result.begin(), result.end());
}

template <typename T> void put(std::vector<unsigned char> &out, const T &value)
{
    const auto *bytes = reinterpret_cast<const unsigned char *>(&value);
    out.insert(out.end(), bytes, bytes + sizeof(T));
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        std::fprintf(stderr, "usage: %s font.ttf out.huifont [pixel_size] [sdf_range] [atlas] [glyphs] [face]\n", argv[0]);
        return 2;
    }
    const float pixel_size = argc > 3 ? std::strtof(argv[3], nullptr) : 56.0f;
    const int range = argc > 4 ? std::atoi(argv[4]) : 8;
    const int atlas_size = argc > 5 ? std::atoi(argv[5]) : 1024;
    // A font collection (.ttc) holds several faces; Noto Sans CJK's first is Japanese.
    const int face = argc > 7 ? std::atoi(argv[7]) : 0;
    if (atlas_size < 64 || atlas_size > 16384)
    {
        std::fprintf(stderr, "atlas %d: use 64 to 16384 (the header and GL both limit it)\n", atlas_size);
        return 2;
    }

    std::ifstream input(argv[1], std::ios::binary);
    std::vector<unsigned char> ttf((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    stbtt_fontinfo font;
    const int offset = ttf.empty() ? -1 : stbtt_GetFontOffsetForIndex(ttf.data(), face);
    if (offset < 0 || !stbtt_InitFont(&font, ttf.data(), offset))
    {
        std::fprintf(stderr, "cannot read font %s\n", argv[1]);
        return 1;
    }
    const float scale = stbtt_ScaleForMappingEmToPixels(&font, pixel_size);
    std::set<int> more;
    if (argc > 6 && !add_items(more, argv[6], font, 0))
        return 2;

    std::vector<Baked> glyphs;
    int skipped = 0;
    const auto started = std::chrono::steady_clock::now();
    for (int codepoint : codepoints(more))
    {
        if (codepoint != ' ' && stbtt_FindGlyphIndex(&font, codepoint) == 0)
        {
            // A range names unassigned and missing code points by the thousand.
            if (++skipped <= 20)
                std::fprintf(stderr, "skipping U+%04X (not in font)\n", codepoint);
            continue;
        }
        Baked glyph;
        glyph.codepoint = codepoint;
        int advance = 0;
        int bearing = 0;
        stbtt_GetCodepointHMetrics(&font, codepoint, &advance, &bearing);
        glyph.advance = static_cast<float>(advance) * scale;
        glyph.bitmap = stbtt_GetCodepointSDF(&font, scale, codepoint, range, 128,
                                             128.0f / static_cast<float>(range), &glyph.w, &glyph.h,
                                             &glyph.xoff, &glyph.yoff);
        glyphs.push_back(glyph);
    }

    const double seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    if (skipped > 20)
        std::fprintf(stderr, "skipped %d code points in all\n", skipped);

    // Shelf packing, tallest first, one pixel of spacing.
    std::vector<Baked *> order;
    for (Baked &glyph : glyphs)
        order.push_back(&glyph);
    std::sort(order.begin(), order.end(), [](const Baked *a, const Baked *b) { return a->h > b->h; });
    int pen_x = 1;
    int pen_y = 1;
    int shelf = 0;
    for (Baked *glyph : order)
    {
        if (glyph->bitmap == nullptr)
            continue;
        if (pen_x + glyph->w + 1 > atlas_size)
        {
            pen_x = 1;
            pen_y += shelf + 1;
            shelf = 0;
        }
        if (pen_y + glyph->h + 1 > atlas_size)
        {
            std::fprintf(stderr, "atlas %d is too small\n", atlas_size);
            return 1;
        }
        glyph->x = pen_x;
        glyph->y = pen_y;
        pen_x += glyph->w + 1;
        shelf = std::max(shelf, glyph->h);
    }
    const int atlas_height = std::min(atlas_size, pen_y + shelf + 1);
    std::vector<unsigned char> atlas(static_cast<std::size_t>(atlas_size * atlas_height), 0);
    for (const Baked &glyph : glyphs)
    {
        for (int row = 0; glyph.bitmap != nullptr && row < glyph.h; ++row)
            std::memcpy(&atlas[static_cast<std::size_t>((glyph.y + row) * atlas_size + glyph.x)],
                        glyph.bitmap + row * glyph.w, static_cast<std::size_t>(glyph.w));
    }

    // Every pair is asked, so the cost grows with the square of the glyph
    // count. Ideographs, kana and Hangul sit on a fixed em grid and are not
    // kerned, so pairs from U+2E80 up are left out: a CJK bake would
    // otherwise ask about a billion pairs.
    std::vector<ff::Kern> kerns;
    std::vector<int> indices;
    for (const Baked &glyph : glyphs)
        indices.push_back(glyph.codepoint < 0x2E80 ? stbtt_FindGlyphIndex(&font, glyph.codepoint) : 0);
    for (std::size_t i = 0; i < glyphs.size(); ++i)
    {
        const Baked &a = glyphs[i];
        for (std::size_t j = 0; j < glyphs.size() && indices[i] != 0; ++j)
        {
            const Baked &b = glyphs[j];
            if (indices[j] == 0)
                continue;
            const int kern = stbtt_GetGlyphKernAdvance(&font, indices[i], indices[j]);
            if (kern != 0)
                kerns.push_back(ff::Kern{static_cast<std::uint32_t>(a.codepoint),
                                         static_cast<std::uint32_t>(b.codepoint),
                                         static_cast<float>(kern) * scale});
        }
    }

    int ascent = 0;
    int descent = 0;
    int line_gap = 0;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &line_gap);
    ff::Header header{};
    header.magic = ff::kMagic;
    header.version = ff::kVersion;
    header.atlas_width = static_cast<std::uint16_t>(atlas_size);
    header.atlas_height = static_cast<std::uint16_t>(atlas_height);
    header.pixel_size = pixel_size;
    header.sdf_range = static_cast<float>(range);
    header.ascent = static_cast<float>(ascent) * scale;
    header.descent = static_cast<float>(descent) * scale;
    header.line_gap = static_cast<float>(line_gap) * scale;
    header.glyph_count = static_cast<std::uint32_t>(glyphs.size());
    header.kern_count = static_cast<std::uint32_t>(kerns.size());

    std::vector<unsigned char> out;
    put(out, header);
    std::sort(glyphs.begin(), glyphs.end(), [](const Baked &a, const Baked &b) { return a.codepoint < b.codepoint; });
    for (const Baked &glyph : glyphs)
    {
        ff::Glyph record{};
        record.codepoint = static_cast<std::uint32_t>(glyph.codepoint);
        record.x = static_cast<std::uint16_t>(glyph.x);
        record.y = static_cast<std::uint16_t>(glyph.y);
        record.w = static_cast<std::uint16_t>(glyph.w);
        record.h = static_cast<std::uint16_t>(glyph.h);
        record.offset_x = static_cast<float>(glyph.xoff);
        record.offset_y = static_cast<float>(glyph.yoff);
        record.advance = glyph.advance;
        put(out, record);
    }
    std::sort(kerns.begin(), kerns.end(), [](const ff::Kern &a, const ff::Kern &b) {
        return a.first != b.first ? a.first < b.first : a.second < b.second;
    });
    for (const ff::Kern &kern : kerns)
        put(out, kern);
    out.insert(out.end(), atlas.begin(), atlas.end());

    std::FILE *file = std::fopen(argv[2], "wb");
    if (file == nullptr || std::fwrite(out.data(), 1, out.size(), file) != out.size())
    {
        std::fprintf(stderr, "cannot write %s\n", argv[2]);
        return 1;
    }
    std::fclose(file);
    for (Baked &glyph : glyphs)
        stbtt_FreeSDF(glyph.bitmap, nullptr);
    std::printf("%s: %zu glyphs, %zu kerning pairs, atlas %dx%d, %zu bytes, %.0f us per glyph\n",
                argv[2], glyphs.size(), kerns.size(), atlas_size, atlas_height, out.size(),
                glyphs.empty() ? 0.0 : seconds * 1e6 / static_cast<double>(glyphs.size()));
    return 0;
}
