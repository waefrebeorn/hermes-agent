import sys

with open("/home/wubu/hermes-agent-dev/C/src/tools/web.c", "r") as f:
    c = f.read()

edits = 0

# 1. Schema: add auth_type after include_body line
old_schema_end = ('"include_body":{"type":"boolean","description":"Include response body in output. '
                  'Set false to return only status code and URL (faster, less token usage).",'
                  '"default":true}')
new_schema_end = old_schema_end + ',\n      "auth_type":{"type":"string","description":"Authentication type: \'basic\' (user:pass) or \'bearer\' (token only). Default: basic"}'

# Find in C source: we need to match the literal \" sequences
# In C source, the text looks like \"include_body\":{\"type\":...
# So we search for the actual C bytes
old_c = old_schema_end.replace('"', '\\"')
new_c = new_schema_end.replace('"', '\\"')

# Actually the file content has literal backslash-quote sequences
# Let me just check what the file looks like
idx = c.find('include_body')
if idx < 0:
    print("ERROR: include_body not found!")
    sys.exit(1)
print(f"Found include_body at offset {idx}")
# Show the exact bytes around it
snippet = c[idx:idx+100]
print(f"Snippet: {repr(snippet)}")

# The file should have \" sequences for JSON property names
# Let me find the exact text to replace
# Look for the last property line before the closing }
# Find the include_body...true"} pattern
import re
# Match: ..."include_body":{"type":...,"default":true}"
# followed by newline and the closing structure
pattern = rb'include_body.*?default\\":true\}'
match = re.search(pattern, c.encode('latin-1'))
if match:
    print(f"Regex matched at {match.start()}: {match.group()[:80]}")
else:
    print("Regex did not match")
    # Try without escaping
    match = re.search(rb'include_body[^}]*default.:.true\}', c.encode('latin-1'))
    if match:
        print(f"Simple regex matched: {match.group()[:100]}")

# Let me just do line-based edits since the file is 666 lines
print("\n--- FILE STATS ---")
lines = c.split('\n')
print(f"Total lines: {len(lines)}")

# Find schema end - look for 'end SCHEMA_GET'
for i, line in enumerate(lines):
    if 'include_body' in line and 'default' in line and 'true' in line:
        print(f"Line {i+1}: {line[:120]}...")
    if 'end SCHEMA_GET' in line:
        print(f"Line {i+2}: SCHEMA_GET ends")
