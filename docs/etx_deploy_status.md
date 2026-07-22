# ETX Deploy Status

Date: 2026-07-22

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
- No Servo ON or motion command was executed.

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
