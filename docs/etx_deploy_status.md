# ETX Deploy Status

Date: 2026-07-22

## Git sync

- Task changes were committed locally as `b345fa6 fix: deploy verified 500us six-axis ENI`.
- `git push origin master` stalled in `git-remote-https`; a direct HTTPS probe to
  `github.com:443` timed out after 10 seconds (`curl` exit 28).
- The hung push process was stopped without changing the worktree. Local commits are preserved;
  retry `git push origin master` when GitHub connectivity is restored.

## Verified state

- `6axis_eni/ENI_3_fixed_6axis_500us.xml` contains six `MADLT05BF` slaves and six DC nodes.
- All six `CycleTime0/1` values are `500000 ns`; all six ESC cycle writes are
  `20A1070000000000`; DC activation is `0003`; Drive 1 is the only reference clock.
- The file is semantically identical to `ENI_3.xml` after applying only the intended 500 us
  cycle changes. Its SHA-256 is
  `2a61999da8cfa70835450c08e0030e649ff17b7c429d19ef4546e468f7292449`.
- ETX `/usr/lib/ECPL/Config/Setting.json` is valid JSON with `CycleTime=500` and
  `ENABLE_DC=true`.
- The active `/usr/lib/ECPL/ENI/ENI.xml` has the same SHA-256 as the local candidate.
- Scenario 1 owns EtherCAT: `etx.service` is disabled/inactive and no demo process remains.
- The ARM64 build succeeded. One deployment dry check plus three repeated dry checks all
  connected six drives, reported no drive alarms, confirmed CSP support, set CSP mode, and
  disconnected cleanly.
- The user later reported that three 500 us synchronized forward/back cycles completed with
  `distance=1000000`, `feed=4000000`, `accel=4000000`, and `decel=4000000`.

## 250 us test result

- A structurally valid 250 us candidate was derived from the verified 500 us ENI for the test:
  six DC nodes used `250000 ns`, six ESC cycle writes used `90D0030000000000`, DC activation
  remained `0003`, and Drive 1 remained the only reference clock.
- ETX `Setting.json` was set to 250 us and the ARM64 build succeeded.
- Two no-motion attempts both stopped at `ECPWInit err=6002` (`ECP_ERR_ECAT_SYS`). The ECPW log reported
  `Cycle time: 250 us`, then approximately one-second INT0 waits, SPI CRC `0x0` instead of
  `0x12345678`, and `ECM_InitLibrary fail`.
- No 250 us attempt reached `ECPWConnect`, Servo ON, group setup, or motion.
- The 250 us candidate was removed so it cannot be deployed accidentally. Current ETX firmware
  and hardware must be confirmed by TPM before trying 250 us again.
- The ETX was restored to the verified 500 us Setting and ENI. `ECPWInit` then succeeded.
- A subsequent connection reached all six slaves but Axis 1 reported `0xFF58`, which the Panasonic
  manual maps to `Err88.0` (main-power undervoltage / AC-off detection 2). Check main-circuit
  power, contactor, and phases before clearing the alarm or running motion again.

## Resolved deployment issue

The previous cycle update wrote a literal `\\n` after the closing JSON brace. A later JSON parse
failed with `Extra data`. The deployment script now repairs only this known trailing sequence and
writes a real newline. The repaired ETX file passes `python3 -m json.tool`.

## Repeat the verified no-motion check

```powershell
.\scripts\deploy_etx_6axis_csp.ps1 `
  -Password "<ETX_PASSWORD>" `
  -EniPath ".\6axis_eni\ENI_3_fixed_6axis_500us.xml" `
  -CycleTimeUs 500 `
  -ConfigureCycleTime `
  -ActivateScenario1 `
  -Build `
  -RunDryCheck
```

Before any motion test, confirm emergency stop, limits, clear mechanical space, six-axis direction,
ECPW user-unit conversion, velocity, and acceleration. The candidate has not yet passed motion
testing.
