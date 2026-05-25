#!/usr/bin/env python3
"""Delegate vision analysis to Python Hermes agent via subprocess.
Called by C vision tool when real AI description is needed.
Usage: python3 vision_delegate.py <image_path_or_url> <question>
"""
import subprocess
import sys
import os
import tempfile

def main():
    if len(sys.argv) < 3:
        print("Usage: vision_delegate.py <image> <question>")
        sys.exit(1)

    image = sys.argv[1]
    question = sys.argv[2]

    # For URLs, download first
    local_path = image
    is_url = image.startswith("http://") or image.startswith("https://")
    cleanup = False

    if is_url:
        try:
            import urllib.request
            resp = urllib.request.urlopen(image, timeout=30)
            data = resp.read()
            # Check if it's an actual image (not HTML)
            content_type = resp.headers.get("Content-Type", "")
            if not content_type.startswith("image/"):
                print(f"URL returned {content_type}, not an image")
                sys.exit(1)
            # Write to temp file
            ext = "jpg"
            if content_type.endswith("/png"): ext = "png"
            elif content_type.endswith("/gif"): ext = "gif"
            elif content_type.endswith("/webp"): ext = "webp"
            fd, local_path = tempfile.mkstemp(suffix=f".{ext}")
            os.write(fd, data)
            os.close(fd)
            cleanup = True
        except Exception as e:
            print(f"Download error: {e}")
            sys.exit(1)

    try:
        hermes_bin = os.path.expanduser("~/.hermes/hermes-agent/venv/bin/hermes")
        if not os.path.exists(hermes_bin):
            # Fallback to PATH
            hermes_bin = "hermes"

        result = subprocess.run(
            [hermes_bin, "chat", "-q", question, "--image", local_path],
            capture_output=True, text=True,
            timeout=120  # 2 min for vision analysis
        )

        # Return last 5000 chars of output
        output = result.stdout.strip() or result.stderr.strip()
        if len(output) > 5000:
            output = output[-5000:]
        print(output)
    except subprocess.TimeoutExpired:
        print("Vision analysis timed out (>2 min)")
    except FileNotFoundError:
        print("Python Hermes agent not found. Install hermes-agent or use a vision-capable LLM directly.")
    except Exception as e:
        print(f"Vision delegation error: {e}")
    finally:
        if cleanup and local_path != image:
            try:
                os.unlink(local_path)
            except OSError:
                pass

if __name__ == "__main__":
    main()
