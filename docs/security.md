# Security

OrbitDesktop handles workflows that can create infrastructure, so the production version must treat authentication, authorization, and secrets as first-class features.

## Token Handling

- Prefer OAuth or GitHub App installation tokens over long-lived PATs.
- Store local credentials in Windows Credential Manager (partially prototyped via CredentialVault on Windows).
- Never write raw tokens to logs, files, crash dumps, or console output.
- **In-Memory Security**: Tokens are scrubbed from memory immediately when the session is closed or destroyed using secure volatile pointers (preventing memory leaks/dumps).
- **Shell Injection Mitigation**: Input parameters passed to external utilities (such as branch names and tokens) are escaped dynamically (e.g., via `TextUtil::EscapeShellArg`) to prevent parameter pollution or command injection.
- Use minimum required scopes for repository listing, branch listing, workflow dispatch, and workflow status.

## Authorization

- Use role-based policies for environment creation and destruction.
- Restrict high-cost templates to maintainers or admins.
- Restrict production-like templates to trusted repositories.
- Record actor, repository, branch, template, TTL, and timestamp for every operation.

## Secrets

- Generate temporary database credentials per environment.
- Store Kubernetes secrets outside source control.
- Rotate credentials on every launch.
- Destroy secrets during Nuke and TTL cleanup.

## Supply Chain

- Pin GitHub Actions versions.
- Build images from a known Dockerfile.
- Tag images with commit SHA and environment ID.
- Add vulnerability scanning before deployment in a production pipeline.

