package com.nexdrum.aiora;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.Typeface;

public final class FontAtlas {
    private FontAtlas() {}

    public static Bitmap build(int cellW, int cellH, int columns, int fontPx, int style) {
        final int first = 32;
        final int last = 126;
        final int[] extras = new int[] {
            0x00D7, // ×
            0x00B7, // ·
            0x2013, // –
            0x2014, // —
            0x2192, // →
            0x223F, // ∿
            0x25C0, // ◀
            0x25B6  // ▶
        };
        final int asciiCount = last - first + 1;
        final int count = asciiCount + extras.length;
        final int rows = (count + columns - 1) / columns;

        Bitmap bitmap = Bitmap.createBitmap(
            columns * cellW,
            rows * cellH,
            Bitmap.Config.ARGB_8888);

        Canvas canvas = new Canvas(bitmap);
        canvas.drawColor(Color.TRANSPARENT, PorterDuff.Mode.CLEAR);

        Paint paint = new Paint(
            Paint.ANTI_ALIAS_FLAG |
            Paint.SUBPIXEL_TEXT_FLAG |
            Paint.DITHER_FLAG);
        paint.setColor(Color.WHITE);
        paint.setTextSize(fontPx);
        paint.setTypeface(Typeface.create(
            "sans-serif",
            style == 1 ? Typeface.BOLD : Typeface.NORMAL));

        final float xPad = 6.0f;
        final float baseline = Math.min(cellH - 8.0f, fontPx + 8.0f);

        for (int index = 0; index < count; ++index) {
            int codePoint = index < asciiCount
                ? first + index
                : extras[index - asciiCount];
            int col = index % columns;
            int row = index / columns;
            canvas.drawText(
                new String(Character.toChars(codePoint)),
                col * cellW + xPad,
                row * cellH + baseline,
                paint);
        }
        return bitmap;
    }
}
