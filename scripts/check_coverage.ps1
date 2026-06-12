# check_coverage.ps1
# PostToolUse 훅에서 호출 — 빌드+테스트 성공 후 커버리지를 측정하고
# baseline 대비 하락이 감지되면 Claude에게 테스트 추가를 요청한다.

$root        = $PSScriptRoot | Split-Path -Parent
$tool        = "C:\Program Files\OpenCppCoverage\OpenCppCoverage.exe"
$testExe     = Join-Path $root "x64\Debug\SampleOrderSystemTests.exe"
$sourceDir   = (Resolve-Path (Join-Path $root "src")).Path   # 절대경로 필수 — 상대경로는 CRT 파일까지 매칭됨
$xmlOutput   = Join-Path $root "coverage.xml"
$reportDir   = Join-Path $root "coverage_report"
$baselineFile = Join-Path $root ".coverage_baseline"

# OpenCppCoverage 없으면 조용히 종료
if (-not (Test-Path $tool))    { exit 0 }
if (-not (Test-Path $testExe)) { exit 0 }

# 커버리지 측정 (cobertura XML)
& $tool --sources $sourceDir --export_type "cobertura:$xmlOutput" -- $testExe 2>$null | Out-Null

# HTML 리포트 별도 생성 (stdout 리다이렉트 없이 실행해야 정상 동작)
& $tool --sources $sourceDir --export_type "html:$reportDir" -- $testExe 2>$null | Out-Null

if (-not (Test-Path $xmlOutput)) {
    Write-Host "coverage.xml 생성 실패 — 커버리지 확인 건너뜀."
    exit 0
}

# XML 파싱
[xml]$cov  = Get-Content $xmlOutput -Encoding utf8
$lineRate  = [double]$cov.coverage.'line-rate'
$covered   = [int]$cov.coverage.'lines-covered'
$valid     = [int]$cov.coverage.'lines-valid'
$current   = [math]::Round($lineRate * 100, 1)

# baseline 초기화 또는 읽기
if (-not (Test-Path $baselineFile)) {
    "$current" | Set-Content $baselineFile -Encoding utf8 -NoNewline
    Write-Host "✓ Coverage baseline 초기화: $current% ($covered/$valid lines)"
    exit 0
}

$prev = [math]::Round([double](Get-Content $baselineFile -Raw), 1)

# 유지 또는 상승 — baseline 갱신
if ($current -ge $prev) {
    "$current" | Set-Content $baselineFile -Encoding utf8 -NoNewline
    Write-Host "✓ Coverage: $current% ($covered/$valid lines)  [이전: $prev%]"
    exit 0
}

# ── 커버리지 하락 감지 ────────────────────────────────────────────────────────
$drop = [math]::Round($prev - $current, 1)

$uncovered = @()
foreach ($pkg in $cov.coverage.packages.package) {
    foreach ($cls in $pkg.classes.class) {
        $missCount = ($cls.lines.line | Where-Object { $_.hits -eq '0' } | Measure-Object).Count
        if ($missCount -gt 0) {
            $uncovered += "  - $($cls.filename) (미커버 $missCount 라인)"
        }
    }
}

Write-Host ""
Write-Host "================================================================"
Write-Host "⚠  COVERAGE DROP DETECTED"
Write-Host "   $prev%  →  $current%  (-$drop%)"
Write-Host "   커버된 라인: $covered / $valid"
Write-Host "================================================================"
if ($uncovered.Count -gt 0) {
    Write-Host ""
    Write-Host "미커버 파일:"
    $uncovered | ForEach-Object { Write-Host $_ }
}
Write-Host ""
Write-Host "ACTION: 위 파일의 미커버 코드를 검증하는 테스트를 추가해 커버리지를 $prev% 이상으로 복원하세요."
Write-Host "================================================================"

exit 1
