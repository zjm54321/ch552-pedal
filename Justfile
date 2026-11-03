# List the available development-environment commands.
default:
    @just --list

# Enter the project-local locked shell (also works before the initial commit).
dev:
    nix develop path:.

# Check installed tools without accessing USB devices or flashing firmware.
tools:
    nix develop path:. --command bash --noprofile --norc -euc \
        'sdcc --version; command -v packihx; make --version; wchisp --version; python3 --version; cc --version; just --version'
