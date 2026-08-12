[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $WorkloadPath,

    [Parameter(Mandatory = $true)]
    [string] $DefinitionPath,

    [Parameter(Mandatory = $true)]
    [string] $ExpectedFingerprint,

    [Parameter(Mandatory = $true)]
    [string] $SourceCommit,

    [long] $WorkflowRun = 0,
    [long] $ArtifactId = 0,
    [string] $ExpectedWorkloadSha256 = '',
    [string] $ExpectedDefinitionSha256 = '',
    [string] $ExpectedRunnerSha256 = '',
    [int] $DurationMs = 60000,
    [switch] $SelfTest
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$cpuLimit = 0.5
$rssLimit = 50000000
$minimumPhysicalDurationMs = 60000
$process = $null
$processStarted = $false
$readyPath = Join-Path $env:TEMP (
    "music-rig-shadow-ready-{0}.tmp" -f [Guid]::NewGuid().ToString('N')
)
$capturedAt = (Get-Date).ToUniversalTime().ToString('o')
$samples = [Collections.Generic.List[object]]::new()

function Get-LowerHash([string] $Path) {
    $stream = [IO.File]::OpenRead($Path)
    $algorithm = [Security.Cryptography.SHA256]::Create()
    try {
        $bytes = $algorithm.ComputeHash($stream)
        return [BitConverter]::ToString($bytes).Replace('-', '').ToLowerInvariant()
    } finally {
        $algorithm.Dispose()
        $stream.Dispose()
    }
}

function Quote-ProcessArgument([string] $Value) {
    return '"' + $Value.Replace('"', '\"') + '"'
}

function Add-ProcessSample([Diagnostics.Process] $Target) {
    $Target.Refresh()
    $samples.Add([pscustomobject]@{
        cpu_ticks = $Target.TotalProcessorTime.Ticks
        rss_bytes = [long]$Target.WorkingSet64
        rss_lifetime_peak_bytes = [long]$Target.PeakWorkingSet64
        handles = [int]$Target.HandleCount
        threads = [int]$Target.Threads.Count
    })
}

try {
    if (-not $SelfTest -and $DurationMs -lt $minimumPhysicalDurationMs) {
        throw 'physical measurement requires at least 60000 ms'
    }
    if ($DurationMs -le 0) {
        throw 'duration must be positive'
    }
    foreach ($requiredPath in @($WorkloadPath, $DefinitionPath, $PSCommandPath)) {
        if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
            throw "Required file is missing: $requiredPath"
        }
    }

    $workloadHash = Get-LowerHash $WorkloadPath
    $definitionHash = Get-LowerHash $DefinitionPath
    $runnerHash = Get-LowerHash $PSCommandPath
    if (($ExpectedWorkloadSha256 -and
            $workloadHash -ne $ExpectedWorkloadSha256.ToLowerInvariant()) -or
        ($ExpectedDefinitionSha256 -and
            $definitionHash -ne $ExpectedDefinitionSha256.ToLowerInvariant()) -or
        ($ExpectedRunnerSha256 -and
            $runnerHash -ne $ExpectedRunnerSha256.ToLowerInvariant())) {
        throw 'Transferred file hash does not match the approved input'
    }

    $arguments = @(
        '--definition', (Quote-ProcessArgument $DefinitionPath),
        '--expected-fingerprint', $ExpectedFingerprint,
        '--duration-ms', $DurationMs.ToString([Globalization.CultureInfo]::InvariantCulture),
        '--ready-file', (Quote-ProcessArgument $readyPath)
    ) -join ' '
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $WorkloadPath
    $startInfo.Arguments = $arguments
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    if (-not $process.Start()) {
        throw 'Could not start shadow workload'
    }
    $processStarted = $true

    $readyDeadline = [DateTime]::UtcNow.AddSeconds(5)
    while (-not (Test-Path -LiteralPath $readyPath -PathType Leaf) -and
        -not $process.HasExited -and [DateTime]::UtcNow -lt $readyDeadline) {
        Start-Sleep -Milliseconds 10
    }
    if (-not (Test-Path -LiteralPath $readyPath -PathType Leaf)) {
        throw 'Shadow workload exited or timed out before readiness'
    }

    $window = [Diagnostics.Stopwatch]::StartNew()
    Add-ProcessSample $process
    while (-not $process.WaitForExit(250)) {
        Add-ProcessSample $process
    }
    $window.Stop()
    try {
        Add-ProcessSample $process
    } catch {
        # The final retained sample still covers the idle interval.
    }
    $stdout = $process.StandardOutput.ReadToEnd().Trim()
    $stderr = $process.StandardError.ReadToEnd().Trim()
    if ($process.ExitCode -ne 0 -or $stderr -or [string]::IsNullOrWhiteSpace($stdout)) {
        throw "Shadow workload failed ($($process.ExitCode)): $stderr"
    }
    $workload = $stdout | ConvertFrom-Json

    $first = $samples[0]
    $last = $samples[$samples.Count - 1]
    $cpuTimeNs = [long]([Math]::Max(0, $last.cpu_ticks - $first.cpu_ticks) * 100)
    $observationNs = [long]$workload.observed_duration_ns
    $cpuPercent = [double]$cpuTimeNs * 100.0 / [double]$observationNs
    $peakRss = [long](($samples | Measure-Object rss_bytes -Maximum).Maximum)
    $lifetimePeakRss = [long](
        ($samples | Measure-Object rss_lifetime_peak_bytes -Maximum).Maximum
    )
    $durationPass = $observationNs -ge ([long]$DurationMs * 1000000)
    $cpuPass = $cpuPercent -lt $cpuLimit
    $rssPass = $peakRss -lt $rssLimit
    $zeroActivityPass = (
        $workload.schema -eq 'music-studies/shadow-idle-workload/v1' -and
        $workload.platform -eq 'windows' -and
        $workload.generation -eq 1 -and
        $workload.device_profiles -eq 5 -and
        $workload.output_mode -eq 'suppressed' -and
        $workload.control_requests -eq 0 -and
        $workload.midi_events -eq 0 -and
        $workload.cycles -eq 0 -and
        $workload.mapping_decisions -eq 0 -and
        $workload.suppressed_midi_events -eq 0
    )
    $physical = -not $SelfTest
    $passed = $durationPass -and $zeroActivityPass -and (
        -not $physical -or ($cpuPass -and $rssPass)
    )
    $operatingSystem = Get-CimInstance Win32_OperatingSystem
    $result = [ordered]@{
        schema = 'music-studies/shadow-idle-resource/v1'
        measurement_kind = $(if ($SelfTest) { 'self-test' } else { 'physical-reference' })
        captured_at = $capturedAt
        completed_at = (Get-Date).ToUniversalTime().ToString('o')
        source_commit = $SourceCommit
        platform = 'windows'
        reference_machine = [ordered]@{
            hostname = $env:COMPUTERNAME
            operating_system = $operatingSystem.Caption
            release = $operatingSystem.Version
            architecture = $operatingSystem.OSArchitecture
        }
        hosted_build = [ordered]@{
            workflow_run = $WorkflowRun
            artifact_id = $ArtifactId
        }
        definition = [ordered]@{
            bytes = (Get-Item -LiteralPath $DefinitionPath).Length
            sha256 = $definitionHash
            fingerprint = $ExpectedFingerprint
        }
        workload_artifact = [ordered]@{
            bytes = (Get-Item -LiteralPath $WorkloadPath).Length
            sha256 = $workloadHash
        }
        runner = [ordered]@{
            name = [IO.Path]::GetFileName($PSCommandPath)
            sha256 = $runnerHash
        }
        idle_observation = [ordered]@{
            adapter = 'portable-definition-backed-mock-input'
            wait_primitive = $workload.wait_primitive
            requested_zero_event_duration_ms = $DurationMs
            observed_duration_ns = $observationNs
            measurement_window_ns = [long]($window.Elapsed.TotalMilliseconds * 1000000)
            control_requests = 0
            midi_events = 0
            mapping_decisions = 0
            suppressed_midi_events = 0
            cpu_time_ns = $cpuTimeNs
            cpu_one_core_percent = [Math]::Round($cpuPercent, 6)
            wakeups = [long]$workload.wait_calls
            wakeup_measurement = 'instrumented-native-wait-completions'
            rss_start_bytes = [long]$first.rss_bytes
            rss_peak_observed_bytes = $peakRss
            rss_lifetime_peak_bytes = $lifetimePeakRss
            handles_start = [int]$first.handles
            handles_peak = [int](($samples | Measure-Object handles -Maximum).Maximum)
            threads_start = [int]$first.threads
            threads_peak = [int](($samples | Measure-Object threads -Maximum).Maximum)
            samples = $samples.Count
            exit_code = $process.ExitCode
        }
        thresholds = [ordered]@{
            physical_gates_enforced = $physical
            idle_cpu_one_core_percent_max_exclusive = $cpuLimit
            idle_rss_bytes_max_exclusive = $rssLimit
        }
        evaluation = [ordered]@{
            idle_duration_pass = $durationPass
            idle_cpu_pass = $cpuPass
            idle_rss_pass = $rssPass
            zero_activity_pass = $zeroActivityPass
            cleanup_pass = $process.HasExited
            passed = $passed
        }
        scope = [ordered]@{
            compiled_current_definition_loaded = $true
            output_suppressed = $true
            mock_input_adapter = $true
            audio_or_midi_apis_opened = $false
            audio_or_midi_routes_changed = $false
            service_installed = $false
            live_rig_changes = $false
            stability_counters_observed = $false
        }
    }
    $result | ConvertTo-Json -Depth 10 -Compress
    if (-not $passed) {
        exit 1
    }
} finally {
    if ($processStarted -and -not $process.HasExited) {
        $process.Kill()
        $process.WaitForExit(5000) | Out-Null
    }
    if (Test-Path -LiteralPath $readyPath) {
        Remove-Item -LiteralPath $readyPath -Force
    }
}
