# Slermes C — Testing (May 21 PM v3)

## Commands
```bash
cd /home/wubu/hermes-agent-dev/C/
bash test_runner.sh              # 21 tests (expect 0 fail)
make test-libs                   # Library unit tests only
make clean && make -j$(nproc)    # Build verification
bash ~/hermes-agent-dev/parity_loop.sh   # C/ ↔ slermes/ parity
```

## Smoke Tests
```bash
./hermes                         # Interactive → /help
./hermes --version               # "WuBu Hermes v0.14.0-wubu"
./hermes gateway                 # "[gateway] WuBu Hermes Gateway v..."
./hermes "Say hi"                # One-shot (needs API key in ~/.slermes/.env)
slermes "Say hi"                 # Same binary via alias
```

## Verified
- 21/21 tests passing
- Live DeepSeek API verified (~1.2s)
- Chunked TE decoding verified
- Content-Type header fixed
- Skin "default" skip verified
- Config isolation (~/.slermes/ vs ~/.hermes/)
- Security approval blocks dangerous commands in non-interactive mode
- Browser tools register and respond

## Gaps
- No live API integration test in CI (needs key)
- No mock server for offline testing
- No performance benchmarks
- No memory leak detection (valgrind)
- No security penetration tests
