# FinOps

Preview environments are valuable because they reduce delivery friction, but they can become expensive if resources are forgotten.

## Controls

- TTL is mandatory for every environment.
- Nuke is always visible for active environments.
- Resources include labels for owner, repository, branch, template, and expiration.
- Expired environments are marked for automated cleanup.
- Hourly cost estimates are shown in the desktop dashboard.

## Recommended Labels

```text
orbitdesktop.io/environment-id
orbitdesktop.io/owner
orbitdesktop.io/repository
orbitdesktop.io/branch
orbitdesktop.io/template
orbitdesktop.io/expires-at
```

## Cost Model

The prototype uses static hourly estimates per template. A production version could query cloud billing APIs or maintain an internal cost catalog for compute, storage, database, ingress, and registry usage.

