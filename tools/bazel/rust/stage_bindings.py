"""Copy generated Rust bindings into the declared Cargo OUT_DIR layout."""

from pathlib import Path
import sys


def main() -> None:
    source = Path(sys.argv[1])
    output_directory = Path(sys.argv[2])
    output_directory.mkdir(parents=True, exist_ok=True)
    (output_directory / "bindings.rs").write_bytes(source.read_bytes())


if __name__ == "__main__":
    main()
