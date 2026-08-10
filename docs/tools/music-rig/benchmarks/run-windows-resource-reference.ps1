[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $RunDirectory,

    [Parameter(Mandatory = $true)]
    [string] $ProbeSha256,

    [Parameter(Mandatory = $true)]
    [string] $CliSha256,

    [Parameter(Mandatory = $true)]
    [string] $RunnerSha256,

    [Parameter(Mandatory = $true)]
    [string] $SourceCommit,

    [Parameter(Mandatory = $true)]
    [long] $WorkflowRun,

    [Parameter(Mandatory = $true)]
    [long] $ArtifactId
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$probeName = 'music_rig_windows_resource_measurement_test'
$cliName = 'music-rig'
$probePath = Join-Path $RunDirectory "$probeName.exe"
$cliPath = Join-Path $RunDirectory "$cliName.exe"
$capturedAt = (Get-Date).ToUniversalTime().ToString('o')
$measurement = $null
$failure = $null
$preflightProbeProcesses = 0
$preflightCliProcesses = 0
$probeBytes = 0
$cliBytes = 0
$cleanup = [ordered]@{
    remaining_probe_processes = $null
    remaining_cli_processes = $null
    temporary_directory_removed = $false
}

try {
    $resolvedTemp = [IO.Path]::GetFullPath($env:TEMP).TrimEnd('\') + '\'
    $resolvedRun = [IO.Path]::GetFullPath($RunDirectory).TrimEnd('\') + '\'
    if (-not $resolvedRun.StartsWith(
        $resolvedTemp,
        [StringComparison]::OrdinalIgnoreCase
    )) {
        throw 'Run directory must be below the current user TEMP directory'
    }

    foreach ($requiredPath in @($probePath, $cliPath, $PSCommandPath)) {
        if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
            throw "Required file is missing: $requiredPath"
        }
    }

    $actualProbeHash = (
        Get-FileHash -Algorithm SHA256 -LiteralPath $probePath
    ).Hash.ToLowerInvariant()
    $actualCliHash = (
        Get-FileHash -Algorithm SHA256 -LiteralPath $cliPath
    ).Hash.ToLowerInvariant()
    $actualRunnerHash = (
        Get-FileHash -Algorithm SHA256 -LiteralPath $PSCommandPath
    ).Hash.ToLowerInvariant()
    if ($actualProbeHash -ne $ProbeSha256.ToLowerInvariant() -or
        $actualCliHash -ne $CliSha256.ToLowerInvariant() -or
        $actualRunnerHash -ne $RunnerSha256.ToLowerInvariant()) {
        throw 'Transferred file hash does not match the approved input'
    }

    $probeBytes = (Get-Item -LiteralPath $probePath).Length
    $cliBytes = (Get-Item -LiteralPath $cliPath).Length
    $preflightProbeProcesses = @(
        Get-Process -Name $probeName -ErrorAction SilentlyContinue
    ).Count
    $preflightCliProcesses = @(
        Get-Process -Name $cliName -ErrorAction SilentlyContinue
    ).Count
    if ($preflightProbeProcesses -ne 0 -or $preflightCliProcesses -ne 0) {
        throw 'A test process already exists before measurement'
    }

    $rawOutput = @(& $probePath --measure $cliPath 60000)
    $probeExitCode = $LASTEXITCODE
    $rawJson = ($rawOutput -join [Environment]::NewLine).Trim()
    if ([string]::IsNullOrWhiteSpace($rawJson)) {
        throw 'Resource probe emitted no result'
    }
    $measurement = $rawJson | ConvertFrom-Json
    if ($probeExitCode -ne 0 -or
        $measurement.schema -ne
            'music-studies/windows-resource-measurement/v1' -or
        $measurement.measurement_kind -ne 'physical-reference' -or
        -not $measurement.thresholds.physical_gates_enforced -or
        $measurement.idle_daemon.requested_zero_event_duration_ms -lt 60000 -or
        $measurement.idle_daemon.control_requests -ne 0 -or
        $measurement.idle_daemon.midi_events -ne 0 -or
        $measurement.short_process.handles_peak -lt 1 -or
        $measurement.short_process.threads_peak -lt 1 -or
        $measurement.idle_daemon.handles_peak -lt 1 -or
        $measurement.idle_daemon.threads_peak -lt 1 -or
        -not $measurement.evaluation.short_process_pass -or
        -not $measurement.evaluation.idle_duration_pass -or
        -not $measurement.evaluation.idle_cpu_pass -or
        -not $measurement.evaluation.idle_working_set_pass -or
        -not $measurement.evaluation.zero_activity_pass -or
        -not $measurement.evaluation.cleanup_pass -or
        -not $measurement.evaluation.passed -or
        -not $measurement.scope.synthetic_only -or
        $measurement.scope.audio_or_midi_apis_opened -or
        $measurement.scope.audio_or_midi_routes_changed -or
        $measurement.scope.service_installed -or
        $measurement.scope.live_rig_changes) {
        throw 'Resource measurement did not satisfy its contract'
    }
} catch {
    $failure = $_.Exception.Message
} finally {
    Start-Sleep -Milliseconds 250
    $cleanup.remaining_probe_processes = @(
        Get-Process -Name $probeName -ErrorAction SilentlyContinue
    ).Count
    $cleanup.remaining_cli_processes = @(
        Get-Process -Name $cliName -ErrorAction SilentlyContinue
    ).Count
    try {
        if (Test-Path -LiteralPath $RunDirectory) {
            Remove-Item -LiteralPath $RunDirectory -Recurse -Force
        }
        $cleanup.temporary_directory_removed =
            -not (Test-Path -LiteralPath $RunDirectory)
    } catch {
        if ($null -eq $failure) {
            $failure = $_.Exception.Message
        } else {
            $failure = "$failure; cleanup: $($_.Exception.Message)"
        }
    }
}

if ($cleanup.remaining_probe_processes -ne 0 -or
    $cleanup.remaining_cli_processes -ne 0 -or
    -not $cleanup.temporary_directory_removed) {
    if ($null -eq $failure) {
        $failure = 'Post-run process or directory cleanup failed'
    }
}
if ($null -ne $failure) {
    throw $failure
}

$operatingSystem = Get-CimInstance Win32_OperatingSystem
[ordered]@{
    schema = 'music-studies/windows-resource-reference-run/v1'
    captured_at = $capturedAt
    completed_at = (Get-Date).ToUniversalTime().ToString('o')
    source_commit = $SourceCommit
    reference_machine = [ordered]@{
        hostname = $env:COMPUTERNAME
        operating_system = $operatingSystem.Caption
        version = $operatingSystem.Version
        architecture = $operatingSystem.OSArchitecture
    }
    hosted_build = [ordered]@{
        workflow_run = $WorkflowRun
        artifact_id = $ArtifactId
    }
    artifacts = [ordered]@{
        probe = [ordered]@{
            name = "$probeName.exe"
            bytes = $probeBytes
            sha256 = $ProbeSha256.ToLowerInvariant()
        }
        cli = [ordered]@{
            name = "$cliName.exe"
            bytes = $cliBytes
            sha256 = $CliSha256.ToLowerInvariant()
        }
        runner = [ordered]@{
            name = [IO.Path]::GetFileName($PSCommandPath)
            sha256 = $RunnerSha256.ToLowerInvariant()
        }
        hashes_verified_on_reference_machine = $true
    }
    preflight = [ordered]@{
        existing_probe_processes = $preflightProbeProcesses
        existing_cli_processes = $preflightCliProcesses
    }
    measurement = $measurement
    cleanup = $cleanup
    scope = [ordered]@{
        repository_copied_to_reference_machine = $false
        temporary_files_only = $true
        audio_or_midi_apis_opened = $false
        audio_or_midi_routes_changed = $false
        services_changed = $false
        startup_entries_changed = $false
        live_rig_changes = $false
    }
    gate = [ordered]@{
        milestone_0_windows_resources = 'pass'
    }
} | ConvertTo-Json -Depth 12 -Compress
