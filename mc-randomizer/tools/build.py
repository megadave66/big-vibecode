#!/usr/bin/env python3
"""Build and package the mc-randomizer behavior pack.

Usage:
    python3 tools/build.py [--check] [--skip-typecheck] [--no-generate]

Flags:
    --check         Run all checks without creating the zip file.
    --skip-typecheck    Skip TypeScript type-checking.
    --no-generate   Skip running generators.

Exit code 1 on any failure, 0 on success.
"""

import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import uuid
import zipfile
from pathlib import Path


def get_root():
    """Get project root: parent of the tools directory."""
    return Path(__file__).parent.parent


def get_args():
    """Parse command-line arguments."""
    return {
        "check": "--check" in sys.argv,
        "skip_typecheck": "--skip-typecheck" in sys.argv,
        "no_generate": "--no-generate" in sys.argv,
    }


def print_result(step_num, name, status, details=""):
    """Print a step result line."""
    line = f"Step {step_num}: {name} ... {status}"
    if details:
        print(f"{line}\n  {details}")
    else:
        print(line)


def step_1_generate(root, no_generate):
    """Step 1: Run generators if they exist."""
    print("\n--- Step 1: Generate ---")

    if no_generate:
        print_result(1, "Generate", "SKIP (--no-generate)")
        return True, ""

    gen_item_universe = root / "tools" / "gen_item_universe.py"
    gen_data_domains = root / "tools" / "gen_data_domains.py"

    errors = []

    # gen_item_universe.py is required
    if not gen_item_universe.exists():
        return False, "gen_item_universe.py not found"

    try:
        result = subprocess.run(
            [sys.executable, str(gen_item_universe)],
            cwd=root,
            capture_output=True,
            text=True,
            timeout=120,
        )
        if result.returncode != 0:
            stderr_tail = "\n".join(result.stderr.split("\n")[-5:])
            return False, f"gen_item_universe.py failed:\n{stderr_tail}"
        print_result(1, "gen_item_universe.py", "PASS")
    except Exception as e:
        return False, f"gen_item_universe.py error: {e}"

    # gen_data_domains.py is required
    if not gen_data_domains.exists():
        return False, "gen_data_domains.py not found"

    try:
        result = subprocess.run(
            [sys.executable, str(gen_data_domains)],
            cwd=root,
            capture_output=True,
            text=True,
            timeout=120,
        )
        if result.returncode != 0:
            stderr_tail = "\n".join(result.stderr.split("\n")[-5:])
            return False, f"gen_data_domains.py failed:\n{stderr_tail}"
        print_result(1, "gen_data_domains.py", "PASS")
    except Exception as e:
        return False, f"gen_data_domains.py error: {e}"

    return True, ""


def step_2_validate_json(root):
    """Step 2: Validate all JSON files in behavior_pack/ and data/."""
    print("\n--- Step 2: Validate JSON ---")

    pack = root / "behavior_pack"
    data = root / "data"

    errors = []

    def parse_constant_rejector(s):
        """Reject NaN/Infinity in JSON."""
        raise ValueError(f"JSON contains {s}")

    # Check behavior_pack/ JSON files
    if pack.exists():
        for f in pack.rglob("*.json"):
            try:
                text = f.read_text(encoding="utf-8")
                json.loads(text, parse_constant=parse_constant_rejector)
            except UnicodeDecodeError as e:
                errors.append(f"{f.relative_to(root)}: {e}")
            except json.JSONDecodeError as e:
                errors.append(f"{f.relative_to(root)}: {e}")
            except ValueError as e:
                errors.append(f"{f.relative_to(root)}: {e}")

    # Check data/ JSON files
    if data.exists():
        for f in data.rglob("*.json"):
            try:
                text = f.read_text(encoding="utf-8")
                json.loads(text, parse_constant=parse_constant_rejector)
            except UnicodeDecodeError as e:
                errors.append(f"{f.relative_to(root)}: {e}")
            except json.JSONDecodeError as e:
                errors.append(f"{f.relative_to(root)}: {e}")
            except ValueError as e:
                errors.append(f"{f.relative_to(root)}: {e}")

    if errors:
        msg = "\n".join(errors)
        if len(errors) > 5:
            msg = "\n".join(errors[:5]) + f"\n... and {len(errors)-5} more"
        return False, msg

    print_result(2, "JSON parsing", "PASS")
    return True, ""


def step_3_validate_manifest(root):
    """Step 3: Validate manifest.json."""
    print("\n--- Step 3: Validate Manifest ---")

    manifest_path = root / "behavior_pack" / "manifest.json"

    if not manifest_path.exists():
        return False, "manifest.json not found"

    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as e:
        return False, f"manifest.json parse error: {e}"

    errors = []

    # Check format_version is 2
    if manifest.get("format_version") != 2:
        errors.append("format_version must be 2")

    # Check header
    header = manifest.get("header", {})
    if not header:
        errors.append("header is required")
    else:
        header_uuid = header.get("uuid")
        if not header_uuid:
            errors.append("header.uuid is required")
        else:
            try:
                u = uuid.UUID(header_uuid)
                if u.version != 4:
                    errors.append(f"header.uuid must be v4, got v{u.version}")
            except ValueError:
                errors.append(f"header.uuid is invalid: {header_uuid}")

        # Check min_engine_version is exactly [1, 21, 0]
        min_engine = header.get("min_engine_version")
        if min_engine != [1, 21, 0]:
            errors.append(f"min_engine_version must be [1, 21, 0], got {min_engine}")

        # Check version is an array
        version = header.get("version")
        if not isinstance(version, list):
            errors.append(f"header.version must be an array")

    # Check modules
    modules = manifest.get("modules", [])
    if not modules:
        errors.append("modules is required")
    else:
        module_uuids = set()
        module_types = set()

        for mod in modules:
            mod_uuid = mod.get("uuid")
            if mod_uuid:
                try:
                    u = uuid.UUID(mod_uuid)
                    if u.version != 4:
                        errors.append(f"module uuid must be v4, got v{u.version}")
                    if mod_uuid in module_uuids:
                        errors.append("module uuids must be distinct")
                    module_uuids.add(mod_uuid)
                except ValueError:
                    errors.append(f"module uuid is invalid: {mod_uuid}")

            # Check version is array
            mod_version = mod.get("version")
            if not isinstance(mod_version, list):
                errors.append(f"module.version must be an array")

            mod_type = mod.get("type")
            module_types.add(mod_type)

            # Check script module language
            if mod_type == "script":
                if mod.get("language") != "javascript":
                    errors.append(f"script module language must be 'javascript'")

        # Check we have both data and script modules
        if "data" not in module_types or "script" not in module_types:
            errors.append("must have both 'data' and 'script' modules")

        # Check script module has entry point
        for mod in modules:
            if mod.get("type") == "script":
                entry = mod.get("entry")
                if not entry:
                    errors.append("script module must have 'entry' field")
                else:
                    entry_path = root / "behavior_pack" / entry
                    if not entry_path.exists():
                        errors.append(f"script entry file not found: {entry}")

    # Check header uuid and all module uuids are distinct
    all_uuids = [header.get("uuid")] + [mod.get("uuid") for mod in modules if mod.get("uuid")]
    if len(set(all_uuids)) != len(all_uuids):
        errors.append("all uuids (header + modules) must be distinct")

    # Check dependencies
    deps = {d.get("module_name"): d.get("version") for d in manifest.get("dependencies", [])}
    if deps.get("@minecraft/server") != "1.11.0":
        errors.append(f"@minecraft/server version must be 1.11.0, got {deps.get('@minecraft/server')}")

    if errors:
        return False, "; ".join(errors[:3])

    print_result(3, "Manifest", "PASS")
    return True, ""


def step_4_consistency(root):
    """Step 4: Validate item universe and exclusions."""
    print("\n--- Step 4: Validate Consistency ---")

    data = root / "data"

    # Load item_universe.json (required)
    universe_path = data / "item_universe.json"
    if not universe_path.exists():
        return False, "item_universe.json not found"

    try:
        universe = json.loads(universe_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as e:
        return False, f"item_universe.json parse error: {e}"

    if not isinstance(universe, list):
        return False, "item_universe.json must be an array"

    errors = []
    universe_set = set(universe)

    # Check sorted and unique
    sorted_universe = sorted(set(universe))
    if universe != sorted_universe:
        errors.append("item_universe must be sorted and unique")

    # Check all IDs match pattern
    id_pattern = re.compile(r"^minecraft:[a-z0-9_.]+$")
    for item_id in universe:
        if not id_pattern.match(item_id):
            errors.append(f"invalid id format: {item_id}")
            if len(errors) > 5:
                break

    # Load vanilla_items_1.21.0.json (required)
    vanilla_path = data / "vanilla_items_1.21.0.json"
    if not vanilla_path.exists():
        return False, "vanilla_items_1.21.0.json not found"

    try:
        vanilla_items = set(json.loads(vanilla_path.read_text(encoding="utf-8")))
    except json.JSONDecodeError as e:
        return False, f"vanilla_items_1.21.0.json parse error: {e}"

    missing_in_vanilla = [i for i in universe if i not in vanilla_items]
    if missing_in_vanilla:
        errors.append(f"items not in vanilla: {missing_in_vanilla[:2]}")

    # Load and check exclusions (required)
    exclusions_path = data / "item_exclusions.json"
    if not exclusions_path.exists():
        return False, "item_exclusions.json not found"

    try:
        exclusions = json.loads(exclusions_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as e:
        return False, f"item_exclusions.json parse error: {e}"

    if not isinstance(exclusions, dict):
        return False, "item_exclusions.json must be an object"

    overlap = universe_set & set(exclusions.keys())
    if overlap:
        errors.append(f"items in both universe and exclusions: {list(overlap)[:2]}")

    # Load build_config.json (required)
    build_config_path = data / "build_config.json"
    if not build_config_path.exists():
        return False, "build_config.json not found"

    try:
        build_config = json.loads(build_config_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as e:
        return False, f"build_config.json parse error: {e}"

    # Validate permutation with multiple seeds
    try:
        sys.path.insert(0, str(root / "tools"))
        import remap_engine

        build_seed_str = build_config.get("build_seed", "mc-randomizer-build-v1")
        build_seed = remap_engine.hash_string(build_seed_str)

        # Check bijection and zero identity for specified seeds
        for seed in [0, 1, 42, 2**32 - 1, build_seed]:
            perm = remap_engine.build_permutation(seed, universe)
            if set(perm.keys()) != universe_set:
                errors.append(f"permutation seed {seed} incomplete")
                break
            if set(perm.values()) != universe_set:
                errors.append(f"permutation seed {seed} not bijection")
                break
            identity_count = sum(1 for k, v in perm.items() if k == v)
            if identity_count > 0:
                errors.append(f"permutation seed {seed}: {identity_count} identities")
                break
    except ImportError:
        return False, "remap_engine.py not found"
    except Exception as e:
        return False, f"permutation check failed: {e}"

    # Check remap_report.json (required)
    report_path = root / "build" / "remap_report.json"
    if not report_path.exists():
        return False, "build/remap_report.json not found"

    try:
        report = json.loads(report_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as e:
        return False, f"remap_report.json parse error: {e}"

    if not isinstance(report, dict):
        return False, "remap_report.json must be an object"

    # Validate report structure
    if "build_seed" not in report or "seed" not in report or "domains" not in report:
        return False, "remap_report.json missing required fields: build_seed, seed, domains"

    if not isinstance(report.get("domains"), dict):
        return False, "remap_report.json domains must be an object"

    expected_domains = {"crafting", "smelting", "chest_loot", "trades"}
    actual_domains = set(report["domains"].keys())
    if actual_domains != expected_domains:
        return False, f"remap_report domains mismatch. expected {expected_domains}, got {actual_domains}"

    # Validate each domain's entries
    report_seed = report["seed"]
    report_perm = remap_engine.build_permutation(report_seed, universe)

    for domain, entries in report["domains"].items():
        if not isinstance(entries, list):
            return False, f"remap_report {domain} must be a list"
        for entry in entries:
            if not isinstance(entry, dict):
                return False, f"remap_report {domain} entry not a dict"
            from_id = entry.get("from")
            to_id = entry.get("to")
            if from_id == to_id:
                errors.append(f"remap_report {domain}: identity remap")
                break
            if from_id not in universe_set:
                errors.append(f"remap_report {domain}: from {from_id} not in universe")
                break
            if to_id not in universe_set:
                errors.append(f"remap_report {domain}: to {to_id} not in universe")
                break
            if to_id != report_perm.get(from_id):
                errors.append(f"remap_report {domain}: {from_id}->{to_id} does not match permutation")
                break

    if errors:
        return False, "; ".join(errors[:2])

    print_result(4, "Consistency", "PASS")
    return True, ""


def step_5_domains(root):
    """Step 5: Validate domains are present in remap_engine.js."""
    print("\n--- Step 5: Validate Domains ---")

    remap_engine_path = root / "behavior_pack" / "scripts" / "remap_engine.js"

    if not remap_engine_path.exists():
        return False, "remap_engine.js not found"

    content = remap_engine_path.read_text(encoding="utf-8")

    errors = []

    # Parse DOMAINS array
    domains_match = re.search(r'export\s+const\s+DOMAINS\s*=\s*(?:Object\.freeze\s*\(\s*)?\[(.*?)\]', content, re.DOTALL)
    if domains_match:
        domains_str = domains_match.group(1)
        domains = [d.strip().strip('"').strip("'") for d in domains_str.split(",") if d.strip()]
    else:
        domains = []

    expected_domains = ["block_drops", "mob_drops", "crafting", "smelting", "chest_loot", "fishing", "trades"]
    if domains != expected_domains:
        errors.append(f"DOMAINS mismatch. expected {expected_domains}, got {domains}")

    # Parse EXCLUDED_ENTITIES array
    excluded_match = re.search(r'export\s+const\s+EXCLUDED_ENTITIES\s*=\s*(?:Object\.freeze\s*\(\s*)?\[(.*?)\]', content, re.DOTALL)
    if excluded_match:
        excluded_str = excluded_match.group(1)
        excluded = [e.strip().strip('"').strip("'") for e in excluded_str.split(",") if e.strip()]
    else:
        excluded = []

    if "minecraft:ender_dragon" not in excluded:
        errors.append("minecraft:ender_dragon not in EXCLUDED_ENTITIES")

    if errors:
        return False, "; ".join(errors)

    print_result(5, "Domains", "PASS")
    return True, ""


def step_6_typecheck(root, skip_typecheck):
    """Step 6: Type-check scripts with TypeScript."""
    print("\n--- Step 6: Type-check ---")

    if skip_typecheck:
        print_result(6, "Type-check", "SKIP (--skip-typecheck)")
        return True, ""

    dev = root / "dev"
    tsc = dev / "node_modules" / ".bin" / "tsc"

    if not tsc.exists():
        return False, f"tsc not found. Run: cd {dev} && npm install"

    tsconfig = dev / "tsconfig.json"

    try:
        result = subprocess.run(
            [str(tsc), "-p", str(tsconfig)],
            capture_output=True,
            text=True,
            timeout=300,
        )
        if result.returncode != 0:
            return False, result.stdout + result.stderr
    except Exception as e:
        return False, f"type-check error: {e}"

    print_result(6, "Type-check", "PASS")
    return True, ""


def step_7_zip(root, check_only, failed_earlier):
    """Step 7: Create the .mcaddon zip file."""
    if check_only:
        print("\n--- Step 7: Zip ---")
        print_result(7, "Zip", "SKIP (--check)")
        return True, ""

    print("\n--- Step 7: Zip ---")

    # If earlier steps failed, skip zip and clean up
    if failed_earlier:
        dist = root / "dist"
        zip_path = dist / "mc-randomizer.mcaddon"
        if zip_path.exists():
            zip_path.unlink()
        print_result(7, "Zip", "SKIP (earlier failure)")
        return False, ""

    dist = root / "dist"
    dist.mkdir(exist_ok=True)

    zip_path = dist / "mc-randomizer.mcaddon"
    pack = root / "behavior_pack"

    # Check pack has required files
    if not (pack / "manifest.json").exists() or not (pack / "scripts").exists():
        if zip_path.exists():
            zip_path.unlink()
        return False, "pack missing manifest.json or scripts/"

    # Create temp zip first
    with tempfile.NamedTemporaryFile(suffix=".zip", delete=False) as tmp:
        tmp_path = tmp.name

    try:
        # Collect files to zip (sorted for determinism)
        files_to_zip = []
        for f in sorted(pack.rglob("*")):
            if f.is_file():
                # Skip unwanted files
                name = f.name
                if name in (".DS_Store",):
                    continue
                if name.endswith(".py"):
                    continue
                if name.startswith("."):
                    continue

                # Check path parts for dotfiles and __pycache__
                parts = f.relative_to(pack).parts
                if any(p.startswith(".") or p == "__pycache__" for p in parts):
                    continue

                files_to_zip.append(f)

        if not files_to_zip:
            if zip_path.exists():
                zip_path.unlink()
            os.unlink(tmp_path)
            return False, "no files to zip"

        # Create zip with deterministic settings
        with zipfile.ZipFile(tmp_path, "w", zipfile.ZIP_DEFLATED) as z:
            for f in files_to_zip:
                rel_path = f.relative_to(pack)
                arc_path = f"mc-randomizer_BP/{rel_path}"

                # Use ZipInfo for deterministic date_time and perms
                info = zipfile.ZipInfo(str(arc_path))
                info.date_time = (1980, 1, 1, 0, 0, 0)
                info.external_attr = 0o644 << 16  # Unix file permissions

                with open(f, "rb") as src:
                    z.writestr(info, src.read(), compress_type=zipfile.ZIP_DEFLATED)

        # Atomic replace
        os.replace(tmp_path, zip_path)

        size_kb = zip_path.stat().st_size / 1024
        print_result(7, "Zip", f"PASS ({len(files_to_zip)} files, {size_kb:.1f} KB)")

        return True, ""

    except Exception as e:
        if os.path.exists(tmp_path):
            os.unlink(tmp_path)
        if zip_path.exists():
            zip_path.unlink()
        return False, f"zip error: {e}"


def main():
    """Main build function."""
    args = get_args()
    root = get_root()

    print("MC-Randomizer Build System")
    print(f"Project root: {root}")

    failed = False

    # Step 1: Generate
    passed, details = step_1_generate(root, args["no_generate"])
    if not passed:
        print_result(1, "Generate", "FAIL", details)
        failed = True

    # Step 2: Validate JSON
    passed, details = step_2_validate_json(root)
    if not passed:
        print_result(2, "JSON parsing", "FAIL", details)
        failed = True

    # Step 3: Validate Manifest
    passed, details = step_3_validate_manifest(root)
    if not passed:
        print_result(3, "Manifest", "FAIL", details)
        failed = True

    # Step 4: Consistency
    passed, details = step_4_consistency(root)
    if not passed:
        print_result(4, "Consistency", "FAIL", details)
        failed = True

    # Step 5: Domains
    passed, details = step_5_domains(root)
    if not passed:
        print_result(5, "Domains", "FAIL", details)
        failed = True

    # Step 6: Type-check
    passed, details = step_6_typecheck(root, args["skip_typecheck"])
    if not passed:
        print_result(6, "Type-check", "FAIL", details)
        failed = True

    # Step 7: Zip
    passed, details = step_7_zip(root, args["check"], failed)
    if not passed and details:
        print_result(7, "Zip", "FAIL", details)
        failed = True

    # Final summary
    print("\n" + "="*50)
    if not failed:
        print("BUILD PASSED")
        return 0
    else:
        print("BUILD FAILED")
        return 1


if __name__ == "__main__":
    sys.exit(main())
