# ETX Deploy Status

Date: 2026-07-16

Local code commit:

- `25b157d feat: add ETX 6-axis CSP demo`

Cloud push:

- Succeeded: `origin/master`

ETX SSH status:

- Host: `192.168.0.10`
- User: `tpm`
- Port 22 reachable
- PuTTY/plink installed at `C:\Program Files\PuTTY`
- Non-interactive plink attempt failed because the available password was not accepted.

Next action:

Run deployment from PowerShell with the correct ETX password:

```powershell
.\scripts\deploy_etx_6axis_csp.ps1 -Password "<ETX_PASSWORD>" -Build -RunDryCheck
```

After dry check passes and machine safety is confirmed:

```powershell
.\scripts\deploy_etx_6axis_csp.ps1 -Password "<ETX_PASSWORD>" -Build -RunMotion
```

