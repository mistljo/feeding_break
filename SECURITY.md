# Security Policy

## Credential Safety

This project is designed with security in mind:

### ✅ What IS in the code (Safe for GitHub)
- API endpoints (public URLs: `cloud.reef-beat.com`, `tunze-hub.com`)
- Red Sea API client authorization header (public OAuth client ID)
- Default WiFi AP password (`ChangeMe123!` - **must be changed before deployment**)
- Code structure and logic

### ❌ What is NOT in the code (Stored securely)
- ❌ Your WiFi password
- ❌ Your Red Sea account credentials
- ❌ Your Tunze account credentials
- ❌ Your Tasmota device IPs/credentials
- ❌ Any personal data

## How Credentials Are Stored

All sensitive data is stored in **ESP32 Flash Memory** (Preferences API):
- WiFi SSID & Password
- Red Sea username/password
- Tunze username/password
- Device configurations

**Never committed to Git** ✓

## Before Publishing

1. **Change Default AP Password**
   - Edit `src/config.h`
   - Change `AP_PASSWORD` from `ChangeMe123!` to something secure
   - This password is used when device creates its own WiFi network

2. **Verify .gitignore**
   - Already configured to exclude sensitive files
   - No credentials should be in tracked files

3. **Review Your Changes**
   ```bash
   git status
   git diff
   ```
   - Make sure no personal data is visible

## Reporting Security Issues

If you discover a security vulnerability:

1. **DO NOT** open a public issue
2. Email the maintainer directly (add your email here)
3. Include:
   - Description of the vulnerability
   - Steps to reproduce
   - Potential impact
   - Suggested fix (if any)

We will respond within 48 hours.

## Security Best Practices

### For Users

1. **Change Default Passwords**
   - AP password in config.h
   - Use strong passwords for all services

2. **Network Security**
   - Device has no authentication on Web Interface
   - Keep it on a trusted network
   - Consider adding network firewall rules

3. **Updates**
   - Check for updates regularly
   - Review changelog for security fixes

4. **Factory Reset**
   - Clears all stored credentials from Flash
   - Use before selling/disposing device

### For Developers

1. **Never Hardcode Secrets**
   - Use Preferences API for all credentials
   - Verify with `git diff` before commit

2. **API Keys**
   - Red Sea OAuth client ID is public (built into their app)
   - No API keys required for Tunze (uses login cookies)

3. **Code Review**
   - All PRs should be reviewed for credential leaks
   - Use GitHub's secret scanning

## Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| Latest  | ✅ Yes             |
| Older   | ❌ No              |

Always use the latest version from `main` branch.

## Acknowledgments

Thank you to the security researchers and contributors who help keep this project secure!
