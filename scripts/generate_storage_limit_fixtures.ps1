param(
  [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\test-data\storage-limit')
)

$ErrorActionPreference = 'Stop'

$deviceTypes = @(
  @{ Name = 'encoder'; Display = 'Encoder'; Code = 1 },
  @{ Name = 'angle'; Display = 'Angle'; Code = 2 },
  @{ Name = 'key'; Display = 'Key'; Code = 3 },
  @{ Name = 'joystick'; Display = 'Joystick'; Code = 4 },
  @{ Name = 'tof'; Display = 'ToF'; Code = 5 }
)

function New-Sequence([string]$address) {
  [ordered]@{ address = $address; type = 1; start = 0; end = 3; step = 1 }
}

function New-Device([hashtable]$type, [int]$index) {
  $number = $index.ToString('D2')
  $uid = $type.Code.ToString('X2') + $index.ToString('X22')
  $root = [ordered]@{
    uid = $uid
    deviceType = $type.Code
    deviceTypeName = $type.Display
    displayName = "Limit $($type.Display) $number"
  }
  $base = "/chainosc/limit/$($type.Name)/$number"
  switch ($type.Name) {
    'key' {
      $root.key = [ordered]@{
        mode = 0
        press = @([ordered]@{ address = $base; value = '1'; type = 1 })
        release = @([ordered]@{ address = $base; value = '0'; type = 1 })
        sequence = New-Sequence "$base/sequence"
      }
    }
    'encoder' {
      $root.encoder = [ordered]@{
        rotationAddress = "$base/rotation"
        sendIncrement = $false
        absoluteInputMin = 0
        absoluteInputMax = 20
        incrementScale = 0.05
        range = [ordered]@{ outMin = 0; outMax = 1; type = 0 }
        clickMode = 0
        press = @([ordered]@{ address = "$base/click"; value = '1'; type = 1 })
        release = @([ordered]@{ address = "$base/click"; value = '0'; type = 1 })
        sequence = New-Sequence "$base/sequence"
      }
    }
    'angle' {
      $root.angle = [ordered]@{
        address = $base
        use12bit = $true
        deadband = 8
        range = [ordered]@{ outMin = 0; outMax = 1; type = 0 }
      }
    }
    'joystick' {
      $root.joystick = [ordered]@{
        xAddress = "$base/x"
        yAddress = "$base/y"
        deadband = 3
        invertX = $false
        invertY = $false
        range = [ordered]@{ outMin = -1; outMax = 1; type = 0 }
        clickMode = 0
        press = @([ordered]@{ address = "$base/click"; value = '1'; type = 1 })
        release = @([ordered]@{ address = "$base/click"; value = '0'; type = 1 })
        sequence = New-Sequence "$base/sequence"
      }
    }
    'tof' {
      $root.tof = [ordered]@{
        address = $base
        deadband = 5
        maxDistanceMm = 2000
        nearValueHigh = $false
        range = [ordered]@{ outMin = 0; outMax = 1; type = 0 }
      }
    }
  }
  $root
}

function New-SettingsFile([object[]]$devices) {
  [ordered]@{
    format = 'M5ChainOSC-settings'
    schemaVersion = 2
    wifiCredentialsIncluded = $false
    global = [ordered]@{
      oscHost = '192.168.1.100'
      oscPort = 9000
      displayRotation = 0
      uiLanguage = 'ja'
    }
    devices = $devices
  }
}

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null

foreach ($type in $deviceTypes) {
  foreach ($count in @(40, 41)) {
    $devices = for ($index = 1; $index -le $count; $index++) {
      New-Device $type $index
    }
    $path = Join-Path $OutputDirectory "M5ChainOSC-settings-$($type.Name)-$count.json"
    New-SettingsFile $devices | ConvertTo-Json -Depth 10 -Compress |
      Set-Content -LiteralPath $path -Encoding utf8
  }
}

$keyType = $deviceTypes | Where-Object Name -eq 'key'
$encoderType = $deviceTypes | Where-Object Name -eq 'encoder'
$mixedDevices = @(
  @(1..40 | ForEach-Object { New-Device $keyType $_ })
  @(New-Device $encoderType 1)
)
$mixedPath = Join-Path $OutputDirectory 'M5ChainOSC-settings-mixed-key40-encoder1.json'
New-SettingsFile $mixedDevices | ConvertTo-Json -Depth 10 -Compress |
  Set-Content -LiteralPath $mixedPath -Encoding utf8

Write-Host "Generated storage-limit fixtures in: $OutputDirectory"
