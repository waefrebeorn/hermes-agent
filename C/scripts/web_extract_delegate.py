#!/usr/bin/env python3
"""
web_extract_delegate.py — F21: Structured web extraction via LLM.

Usage:
  echo '{"url": "...", "prompt": "extract key metrics"}' | python3 web_extract_delegate.py

Fetches URL content, sends to Hermes agent for structured extraction.
"""
import json
import sys
import subprocess
import urllib.request
import urllib.error

def fetch_url(url, timeout=30):
    """Fetch URL content with timeout."""
    try:
        req = urllib.request.Request(url, headers={
            'User-Agent': 'Hermes-C/1.0 (web_extract_delegate)'
        })
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            content = resp.read().decode('utf-8', errors='replace')
            return {
                'status': resp.status,
                'content': content[:50000],  # Cap at 50K chars
                'content_length': len(content)
            }
    except urllib.error.HTTPError as e:
        return {'status': e.code, 'content': str(e.reason), 'content_length': 0}
    except Exception as e:
        return {'status': 0, 'content': str(e), 'content_length': 0}

def main():
    try:
        args = json.loads(sys.stdin.read())
    except (json.JSONDecodeError, IndexError):
        print(json.dumps({'error': 'Invalid JSON input'}))
        sys.exit(1)

    url = args.get('url', '')
    extraction_prompt = args.get('prompt', 'Extract key information from this page content')
    timeout = args.get('timeout', 30)

    if not url:
        print(json.dumps({'error': 'Missing url'}))
        sys.exit(1)

    # Fetch URL
    fetch_result = fetch_url(url, timeout)
    content = fetch_result['content']
    status = fetch_result['status']

    if status != 200 or not content:
        print(json.dumps({
            'url': url,
            'status': status,
            'error': f'Failed to fetch: HTTP {status}',
            'extraction': None
        }))
        return

    # Build extraction prompt
    system_prompt = (
        "You are a web content extraction assistant. "
        "Extract structured information from the provided web page content. "
        f"Extraction instruction: {extraction_prompt}\n\n"
        "Return your response as structured text (JSON, bullet points, or "
        "clear sections). Focus on factual content from the page."
    )

    user_message = (
        f"URL: {url}\n\n"
        f"Page content ({fetch_result['content_length']} chars, truncated to 50K):\n\n"
        f"{content}\n\n"
        f"Extraction task: {extraction_prompt}"
    )

    # Call Hermes agent via CLI
    try:
        proc = subprocess.run(
            ['hermes', 'chat', '--system', system_prompt, '--prompt', user_message,
             '--quiet', '--no-stream'],
            capture_output=True, text=True, timeout=120
        )
        response = proc.stdout.strip()
        if not response:
            response = proc.stderr.strip() or 'No response from agent'
    except subprocess.TimeoutExpired:
        response = 'LLM extraction timed out'
    except FileNotFoundError:
        response = 'hermes CLI not found'
    except Exception as e:
        response = str(e)

    # Return structured result
    result = {
        'url': url,
        'status': status,
        'content_length': fetch_result['content_length'],
        'extraction': response
    }
    print(json.dumps(result))

if __name__ == '__main__':
    main()
