# Project workflow

- Preserve previous firmware, code, configuration, and documentation. Before replacing existing files, archive them outside this repository in ../../FirmwareArchives; never delete the user's historical copies.
- For each requested project update, run appropriate checks, commit the completed changes with a descriptive message, and push to the configured GitHub remote. The user has authorized this ongoing workflow. Report explicitly if a push fails or access is unavailable; never claim unsynced work is on GitHub.
- Inspect staged changes for credentials before committing. Keep device passwords, ESPHome secrets, private credential archives, generated files, build directories, and sensor exports out of GitHub.
- Do not force-push, rewrite published history, or overwrite unrelated remote changes.
- Retain the current device log format and history when updating firmware unless the user explicitly requests otherwise.
