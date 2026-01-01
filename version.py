"""
Build script for PlatformIO to automatically extract version info from Git
Version is auto-generated: YYYY.MM.BuildNumber (e.g., 2026.01.42)
"""
import subprocess
import os
from datetime import datetime

Import("env")

def get_commit_count():
    """Get total number of commits"""
    try:
        count = subprocess.check_output(
            ["git", "rev-list", "--count", "HEAD"],
            stderr=subprocess.DEVNULL
        ).decode().strip()
        return int(count)
    except:
        return 0

def get_git_commit():
    """Get short commit hash"""
    try:
        commit = subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"],
            stderr=subprocess.DEVNULL
        ).decode().strip()
        return commit
    except:
        return "unknown"

def is_dirty():
    """Check if there are uncommitted changes"""
    try:
        result = subprocess.check_output(
            ["git", "status", "--porcelain"],
            stderr=subprocess.DEVNULL
        ).decode().strip()
        return len(result) > 0
    except:
        return False

def get_build_date():
    """Get build date"""
    return datetime.now().strftime("%Y-%m-%d %H:%M")

# Get version info
now = datetime.now()
commit_count = get_commit_count()
commit = get_git_commit()
dirty = is_dirty()

# Auto-generate version: YYYY.MM.BuildNumber
version = f"{now.year}.{now.month:02d}.{commit_count}"
if dirty:
    version += "-dev"

build_date = get_build_date()

print(f"")
print(f"=== Auto Version Info ===")
print(f"Version: {version}")
print(f"Commit:  {commit}")
print(f"Date:    {build_date}")
print(f"=========================")
print(f"")

# Add build flags with version info
env.Append(CPPDEFINES=[
    ("BUILD_VERSION", f'\\"{version}\\"'),
    ("BUILD_COMMIT", f'\\"{commit}\\"'),
    ("BUILD_DATE", f'\\"{build_date}\\"'),
])
