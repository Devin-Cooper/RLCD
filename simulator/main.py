#!/usr/bin/env python3
"""
RLCD Simulator - Toolkit demo runner.

Usage:
    python main.py [--scale N]

Options:
    --scale N   Window scale factor (default: 2)

Controls:
    SPACE   - Cycle demo modes
    1-4     - Jump to specific mode
    S       - Save screenshot
    Q/ESC   - Quit
"""

import argparse
import sys

from demo import run_demo


def main():
    """Main entry point for the RLCD toolkit demo."""
    parser = argparse.ArgumentParser(
        description="RLCD Toolkit Demo",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Controls:
  SPACE   Cycle demo modes
  1-4     Jump to specific mode
  S       Save screenshot
  Q/ESC   Quit

Demo Modes:
  1 - Patterns    Dither pattern showcase in hexagonal shapes
  2 - Bezier      Organic curves with texture-ball strokes
  3 - Numerals    Full digit set at various sizes
  4 - Clock       Composition preview combining all features
""",
    )
    parser.add_argument(
        "--scale",
        type=int,
        default=2,
        metavar="N",
        help="Window scale factor (default: 2)",
    )
    args = parser.parse_args()

    return run_demo(scale=args.scale)


if __name__ == "__main__":
    sys.exit(main())
