"""Command-line entry point: `python -m whilefresh_tools.synthetic generate|evaluate`."""

from __future__ import annotations

import sys


def main() -> int:
    if len(sys.argv) < 2 or sys.argv[1] in ("-h", "--help"):
        print("usage: python -m whilefresh_tools.synthetic {generate|evaluate} [options]")
        return 0 if len(sys.argv) > 1 else 2
    subcommand = sys.argv[1]
    if subcommand == "generate":
        from .generate import main as generate_main

        return generate_main(sys.argv[2:])
    if subcommand == "evaluate":
        from .evaluate import main as evaluate_main

        return evaluate_main(sys.argv[2:])
    print(f"unknown command: {subcommand}", file=sys.stderr)
    return 2


raise SystemExit(main())
