# Build the engine to WebAssembly with Emscripten (Windows PowerShell).
#   . C:\path\to\emsdk\emsdk_env.ps1
#   .\wasm\build.ps1
$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..')
$out = if ($env:OUT) { $env:OUT } else { 'web\public\engine' }
New-Item -ItemType Directory -Force $out | Out-Null
if (-not (Get-Command em++ -ErrorAction SilentlyContinue)) { throw 'em++ not found: run emsdk_env.ps1 first' }
$srcs = Get-ChildItem src\*.cpp | ForEach-Object { $_.FullName }
em++ -O3 -std=c++14 -fwasm-exceptions -Iinclude $srcs -o "$out\cml.js" `
  -s MODULARIZE=1 -s EXPORT_ES6=1 -s EXPORT_NAME=createCmlModule `
  -s ENVIRONMENT=web,worker,node `
  -s ALLOW_MEMORY_GROWTH=1 -s INITIAL_MEMORY=16MB -s MAXIMUM_MEMORY=512MB -s STACK_SIZE=1MB `
  -s FILESYSTEM=0 -s INVOKE_RUN=0 `
  "-s" "EXPORTED_FUNCTIONS=['_cml_execute','_cml_free','_cml_version','_malloc','_free']" `
  "-s" "EXPORTED_RUNTIME_METHODS=['cwrap','UTF8ToString','stringToUTF8','lengthBytesUTF8']"
if ($LASTEXITCODE -ne 0) { throw "em++ failed" }
Get-ChildItem $out | Select-Object Name, Length
