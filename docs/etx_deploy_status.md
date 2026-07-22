# ETX Deploy Status

Date: 2026-07-22

## Completed

- SSH connection to `tpm@192.168.0.10` succeeded with the local credential file, which is
  excluded from this task's commit.
- Scenario resource state: `etx.service` disabled/inactive; TCP 5886 not listening.
- ETX `/usr/lib/ECPL/Config/Setting.json`: `CycleTime=1000`, `ENABLE_DC=true`.
- Updated C++ source uploaded to `/home/tpm/etx_6axis_csp` and built successfully on ARM64.
- Target-side guard test correctly rejected `--cycle-us 500` before ECPW initialization because
  the ETX setting is still 1000 us.

## Blocked before 500 us dry check

- Repository `6axis_eni/ENI.xml` is a valid six-axis DC ENI, but its cycle is still 1000 us.
- Newly exported `C:\TPM\ECPW\ENI\ENI.xml` contains six drives but no DC nodes.
- No ENI or ETX cycle setting was installed or changed, and no motion was executed.

After confirming all axes are stopped and Servo OFF, prepare Scenario 2 for a correct export:

```powershell
.\scripts\deploy_etx_6axis_csp.ps1 `
  -Password "<ETX_PASSWORD>" `
  -CycleTimeUs 500 `
  -PrepareEniExport `
  -ConfirmAxesStoppedAndServoOff
```

Then set all six drives to `DC SYNC0`, `x1`, apply to all, save, export, and replace
`6axis_eni/ENI.xml`. Final Scenario 1 deployment and no-motion check:

```powershell
.\scripts\deploy_etx_6axis_csp.ps1 `
  -Password "<ETX_PASSWORD>" `
  -CycleTimeUs 500 `
  -ConfigureCycleTime `
  -ActivateScenario1 `
  -Build `
  -RunDryCheck
```
