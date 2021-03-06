/* Copyright (c) 2018, Curtis McEnroe <programble@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// Linux framebuffer clock.

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>
#include <zlib.h>

static const uint32_t PSF2_MAGIC = 0x864AB572;
struct Psf2Header {
    uint32_t magic;
    uint32_t version;
    uint32_t headerSize;
    uint32_t flags;
    uint32_t glyphCount;
    uint32_t glyphSize;
    uint32_t glyphHeight;
    uint32_t glyphWidth;
};

static const uint32_t BG = 0x1D2021;
static const uint32_t FG = 0xA99A84;

int main() {
    size_t count;

    const char *fontPath = getenv("FONT");
    if (!fontPath) fontPath = "/usr/share/kbd/consolefonts/Lat2-Terminus16.psfu.gz";

    gzFile font = gzopen(fontPath, "r");
    if (!font) err(EX_NOINPUT, "%s", fontPath);

    struct Psf2Header header;
    count = gzfread(&header, sizeof(header), 1, font);
    if (!count) errx(EX_IOERR, "%s: %s", fontPath, gzerror(font, NULL));

    if (header.magic != PSF2_MAGIC) {
        errx(EX_DATAERR, "%s: invalid header magic %#x", fontPath, header.magic);
    }
    if (header.headerSize != sizeof(struct Psf2Header)) {
        errx(EX_DATAERR, "%s: weird header size %d", fontPath, header.headerSize);
    }

    uint8_t glyphs[128][header.glyphSize];
    count = gzfread(glyphs, header.glyphSize, 128, font);
    if (!count) errx(EX_IOERR, "%s: %s", fontPath, gzerror(font, NULL));

    assert(Z_OK == gzclose(font));

    const char *fbPath = getenv("FRAMEBUFFER");
    if (!fbPath) fbPath = "/dev/fb0";

    int fb = open(fbPath, O_RDWR);
    if (fb < 0) err(EX_OSFILE, "%s", fbPath);

    struct fb_var_screeninfo info;
    int error = ioctl(fb, FBIOGET_VSCREENINFO, &info);
    if (error) err(EX_IOERR, "%s", fbPath);

    size_t len = 4 * info.xres * info.yres;
    uint32_t *buf = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fb, 0);
    if (buf == MAP_FAILED) err(EX_IOERR, "%s", fbPath);

    for (;;) {
        time_t t = time(NULL);
        if (t < 0) err(EX_OSERR, "time");
        const struct tm *local = localtime(&t);
        if (!local) err(EX_OSERR, "localtime");

        char str[64];
        size_t len = strftime(str, sizeof(str), "%H:%M", local);
        assert(len);

        for (int i = 0; i < (60 - local->tm_sec); ++i) {
            uint32_t left = info.xres - header.glyphWidth * len;
            uint32_t bottom = header.glyphHeight;

            for (uint32_t y = 0; y < bottom; ++y) {
                buf[y * info.xres + left - 1] = FG;
            }
            for (uint32_t x = left - 1; x < info.xres; ++x) {
                buf[bottom * info.xres + x] = FG;
            }

            for (const char *s = str; *s; ++s) {
                uint8_t *glyph = glyphs[(uint8_t)*s];
                uint32_t stride = header.glyphSize / header.glyphHeight;
                for (uint32_t y = 0; y < header.glyphHeight; ++y) {
                    for (uint32_t x = 0; x < header.glyphWidth; ++x) {
                        uint8_t bits = glyph[y * stride + x / 8];
                        uint8_t bit = bits >> (7 - x % 8) & 1;
                        buf[y * info.xres + left + x] = bit ? FG : BG;
                    }
                }
                left += header.glyphWidth;
            }

            sleep(1);
        }
    }
}
