#!/usr/bin/env python3
"""Vision analysis helper — color palette, EXIF extraction via PIL."""
import sys, json, collections
from pathlib import Path

def analyze_colors(image_path):
    try:
        from PIL import Image
        img = Image.open(image_path).convert("RGB")
        pixels = list(img.getdata())
        counter = collections.Counter(pixels)
        top = counter.most_common(8)
        colors = []
        for (r, g, b), count in top:
            colors.append({"hex": f"#{r:02x}{g:02x}{b:02x}", "r": r, "g": g, "b": b, "pct": round(count / len(pixels) * 100, 1)})
        return colors
    except Exception as e:
        return {"error": str(e)}

def analyze_exif(image_path):
    try:
        from PIL import Image
        from PIL.ExifTags import TAGS
        img = Image.open(image_path)
        exif = img.getexif()
        result = {}
        for k, v in exif.items():
            tag = TAGS.get(k, str(k))
            result[tag] = str(v)
        return result
    except Exception as e:
        return {"error": str(e)}

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(json.dumps({"error": "Usage: vision_analysis.py <mode> <image_path>"}))
        sys.exit(1)
    mode = sys.argv[1]
    path = sys.argv[2]
    if mode == "colors":
        result = analyze_colors(path)
    elif mode == "exif":
        result = analyze_exif(path)
    else:
        result = {"error": f"Unknown mode: {mode}"}
    print(json.dumps(result))
