# ETX Deploy Status

Date: 2026-07-22

## Git status

- `ebed153 feat: guard 500us six-axis control` was pushed to `origin/master`.
- `7648828 fix: wait for ETX scenario service` was committed locally after the first Scenario 2
  run exposed a startup-delay false failure.
- A push attempt then failed because `github.com:443` was unreachable. Retry with
  `git push origin master`; the local commits are preserved.

## Completed

- SSH connection to `tpm@192.168.0.10` succeeded with the local credential file, which is
  excluded from this task's commit.
- Scenario resource state: `etx.service` enabled/active; TCP 5886 listening.
- ETX `/usr/lib/ECPL/Config/Setting.json`: `CycleTime=500`, `ENABLE_DC=true`.
- ETX service log confirms `Cycle time: 500 us`.
- Updated C++ source uploaded to `/home/tpm/etx_6axis_csp` and built successfully on ARM64.
- An earlier target-side guard test correctly rejected `--cycle-us 500` before ECPW
  initialization while the ETX setting was still 1000 us.

## Blocked before 500 us dry check

- Repository `6axis_eni/ENI.xml` is a valid six-axis DC ENI, but its cycle is still 1000 us.
- Newly exported `C:\TPM\ECPW\ENI\ENI.xml` contains six drives but no DC nodes.
- The ETX setting was backed up and changed to 500 us for Scenario 2 export. No ENI was
  installed, and no dry check or motion was executed.

Scenario 2 preparation has completed. For future repetitions, use:

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
